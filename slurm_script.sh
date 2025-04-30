#!/bin/bash
#SBATCH --job-name=blocking_queue
#SBATCH --partition=Centaurus
#SBATCH --time=00:05:00
#SBATCH --mem-per-cpu=4g

# Load any necessary modules
# module load gcc
# module load curl

# Change to the directory where the executable is located
cd $HOME/ITCS4145/blocking-queue/client

# Run a few example BFS traversals using the parallel executable
./bfs_parallel "Tom Hanks" 4
./bfs_parallel "Mel Gibson" 3
./bfs_parallel "Liam Neeson" 1
