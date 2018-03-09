#!/bin/bash
BUILD_TYPE=${1}
if [ -z "${BUILD_TYPE}" ]; then
    BUILD_TYPE="Release"
fi;

echo $1
set -e
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} ../
make
make test
cd ../
