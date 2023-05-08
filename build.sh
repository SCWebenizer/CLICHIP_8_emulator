#!/bin/bash

# CMake building script V1.0
# Used to make CMake build the project.
# Should be able to be called anywhere, thanks to the directory hopping.

# Paths to temporarily move working directory to the script
current_path=$(pwd)
dir_path=$(dirname $(realpath "${BASH_SOURCE:-$0}"))

# Build project
cd $dir_path
cmake --build build
cd $current_path
