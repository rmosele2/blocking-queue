#!/bin/bash
#SBATCH --job-name=blocking_queue
#SBATCH --partition=Centaurus
#SBATCH --time=01:00:00
#SBATCH --mem-per-cpu=4g
#SBATCH --cpus-per-task=8

# Load any necessary modules
# module load gcc
# module load curl

# Change to the directory where the executable is located
cd $HOME/ITCS4145/blocking-queue

# Run a few example BFS traversals using the parallel executable
./client "Tom Hanks" 4
