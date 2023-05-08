#!/bin/bash

# CMake cache-remaking script V1.0
# Used to allow CMake to build the project.
# Should be able to be called anywhere, thanks to the directory hopping.

# Script arguments will be passed to the cmake commands
# Example useful arguments:
# -D CMAKE_BUILD_TYPE=Debug -D CMAKE_EXPORT_COMPILE_COMMANDS=on
# -D CMAKE_BUILD_TYPE=Release
# -D CMAKE_BUILD_TYPE=RelWithDebInfo
# -D CMAKE_BUILD_TYPE=MinSizeRel
cmake_args=$@
cmake_default_args="-D CMAKE_BUILD_TYPE=Debug -D CMAKE_EXPORT_COMPILE_COMMANDS=on"

# Check if there are any script arguments, and set it to default if there are none
if [ -z "$cmake_args" ]
then
      echo "No cmake arguments added, using default arguments: $cmake_default_args"
      cmake_args=$cmake_default_args
fi

# Paths to temporarily move working directory to the script
current_path=$(pwd)
dir_path=$(dirname $(realpath "${BASH_SOURCE:-$0}"))

# Create/Update cache
cd $dir_path
mkdir -p build
cmake -B build $cmake_args
cd $current_path
