SUITE=$1

if [ "$SUITE" == "unit" ]; then
    cd build
    cmake -D CMAKE_BUILD_TYPE=Release \
        -D IRKit_USE_SYSTEM_BOOST=ON \
        -D IRKit_BUILD_INTEGRATION_TEST=OFF \
        -D IRKit_BUILD_BENCHMARKS=OFF \
        -D IRKit_NO_SANITIZERS=ON \
        ..
    make VERBOSE=1
    ctest -V
elif [ "$SUITE" == "integration" ]; then
    cd build
    cmake -D CMAKE_BUILD_TYPE=Release \
        -D IRKit_USE_SYSTEM_BOOST=ON \
        -D IRKit_BUILD_INTEGRATION_TEST=ON \
        -D IRKit_BUILD_BENCHMARKS=OFF \
        -D IRKit_NO_SANITIZERS=ON \
        ..
    make VERBOSE=1
    ctest -V -R integration*
elif [ "$SUITE" == "benchmarks" ]; then
    cd build
    cmake -D CMAKE_BUILD_TYPE=Release \
        -D IRKit_USE_SYSTEM_BOOST=ON \
        -D IRKit_BUILD_INTEGRATION_TEST=OFF \
        -D IRKit_BUILD_BENCHMARKS=ON \
        -D IRKit_NO_SANITIZERS=ON \
        ..
    make VERBOSE=1
fi
