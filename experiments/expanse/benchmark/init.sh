#!/bin/bash

# exit when any command fails
set -e
# import the the script containing common functions
source ../../include/scripts.sh

# get the LCI source path via environment variable or default value
FB_SOURCE_PATH=$(realpath "${FB_SOURCE_PATH:-../../../}")

if [[ -f "${FB_SOURCE_PATH}/include/comm_exp.hpp" ]]; then
  echo "Found FB at ${FB_SOURCE_PATH}"
else
  echo "Did not find FB at ${FB_SOURCE_PATH}!"
  exit 1
fi

# create the ./init directory
mkdir_s ./init
# move to ./init directory
cd init

# setup module environment
module purge
module load DefaultModules
module load gcc
module load cmake
module load openmpi
module load boost
export CC=gcc
export CXX=g++

# record build status
record_env

mkdir -p log
mv *.log log

# build FB
mkdir -p build
cd build
echo "Running cmake..."
FB_INSTALL_PATH=$(realpath "../install")
cmake -DCMAKE_INSTALL_PREFIX=${FB_INSTALL_PATH} \
      -DCMAKE_BUILD_TYPE=Release \
      -DFB_SERVER=IBV \
      -L \
      ${FB_SOURCE_PATH} | tee init-cmake.log 2>&1 || { echo "cmake error!"; exit 1; }
cmake -LAH . >> init-cmake.log
echo "Running make..."
make VERBOSE=1 -j | tee init-make.log 2>&1 || { echo "make error!"; exit 1; }
#echo "Installing FB to ${FB_INSTALL_PATH}"
#make install > init-install.log 2>&1 || { echo "install error!"; exit 1; }