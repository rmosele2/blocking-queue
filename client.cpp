#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <curl/curl.h>
#include <stdexcept>
#include <rapidjson/document.h>
#include <rapidjson/reader.h>
#include <rapidjson/error/error.h>

using namespace std;
using namespace rapidjson;

// BlockingQueue definition

template <typename T>
class BlockingQueue {
    std::queue<T> q;
    std::mutex m;
    std::condition_variable cv;
    bool finished = false;

public:
    void push(const T& item) {
        {
            std::lock_guard<std::mutex> lock(m);
            q.push(item);
        }
        cv.notify_one();
    }

    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(m);
        cv.wait(lock, [&] { return !q.empty() || finished; });
        if (q.empty()) return false;
        item = q.front();
        q.pop();
        return true;
    }

    void set_finished() {
        {
            std::lock_guard<std::mutex> lock(m);
            finished = true;
        }
        cv.notify_all();
    }
};

bool debug = false;
const string SERVICE_URL = "http://hollywood-graph-crawler.bridgesuncc.org/neighbors/";

string url_encode(CURL* curl, string input) {
    char* out = curl_easy_escape(curl, input.c_str(), input.size());
    string s = out;
    curl_free(out);
    return s;
}

size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* output) {
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

string fetch_neighbors(CURL* curl, const string& node) {
    string url = SERVICE_URL + url_encode(curl, node);
    string response;

    if (debug)
        cout << "Sending request to: " << url << endl;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "User-Agent: C++-Client/1.0");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        cerr << "CURL error: " << curl_easy_strerror(res) << endl;
    } else if (debug) {
        cout << "CURL request successful!" << endl;
    }

    if (debug)
        cout << "Response received: " << response << endl;

    return (res == CURLE_OK) ? response : "{}";
}

vector<string> get_neighbors(const string& json_str) {
    vector<string> neighbors;
    Document doc;
    doc.Parse(json_str.c_str());
    if (doc.HasMember("neighbors") && doc["neighbors"].IsArray()) {
        for (const auto& neighbor : doc["neighbors"].GetArray())
            neighbors.push_back(neighbor.GetString());
    }
    return neighbors;
}

vector<string> bfs_parallel(const string& start, int depth, int num_threads = 8) {
    BlockingQueue<pair<string, int>> q;
    mutex visited_mutex;
    atomic<int> active_threads(0);
    unordered_set<string> visited;
    vector<string> result;
    mutex result_mutex;

    q.push({start, 0});
    visited.insert(start);

    auto worker = [&](CURL* curl) {
        pair<string, int> task;
        while (q.pop(task)) {
            active_threads++;
            auto [node, level] = task;

            {
                lock_guard<mutex> lock(result_mutex);
                result.push_back(node);
            }

            if (level < depth) {
                string response = fetch_neighbors(curl, node);
                for (const auto& neighbor : get_neighbors(response)) {
                    lock_guard<mutex> lock(visited_mutex);
                    if (visited.insert(neighbor).second) {
                        q.push({neighbor, level + 1});
                    }
                }
            }

            active_threads--;
        }
    };

    vector<thread> threads;
    vector<CURL*> curls(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        curls[i] = curl_easy_init();
        threads.emplace_back(worker, curls[i]);
    }

    while (true) {
        this_thread::sleep_for(chrono::milliseconds(100));
        if (active_threads == 0) {
            pair<string, int> dummy;
            if (!q.pop(dummy)) {
                q.set_finished();
                break;
            } else {
                q.push(dummy);
            }
        }
    }

    for (auto& t : threads) t.join();
    for (auto* c : curls) curl_easy_cleanup(c);

    return result;
}

int main(int argc, char* argv[]) {
    if (argc < 3 || argc > 4) {
        cerr << "Usage: " << argv[0] << " <node_name> <depth> [num_threads]\n";
        return 1;
    }

    string start_node = argv[1];
    int depth;
    try {
        depth = stoi(argv[2]);
    } catch (const exception& e) {
        cerr << "Error: Depth must be an integer.\n";
        return 1;
    }

    int num_threads = (argc == 4) ? stoi(argv[3]) : 8;

    const auto start = chrono::steady_clock::now();
    vector<string> result = bfs_parallel(start_node, depth, num_threads);

    for (const auto& node : result)
        cout << "- " << node << "\n";

    const auto finish = chrono::steady_clock::now();
    chrono::duration<double> elapsed_seconds = finish - start;
    cout << "Time to crawl: " << elapsed_seconds.count() << "s\n";

    return 0;
}
