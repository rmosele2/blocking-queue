CXX = g++
CXXFLAGS = -std=c++17 -I$(HOME)/rapidjson/include

all: client

client: client.o
	$(CXX) $(CXXFLAGS) -lcurl -o client client.o

client.o: client.cpp
	$(CXX) $(CXXFLAGS) -c client.cpp

clean:
	rm -f client.o client

