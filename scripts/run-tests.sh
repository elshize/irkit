SUITE=$1

if [ "$SUITE" == "unit" ]; then
    cd build
    cmake -D CMAKE_BUILD_TYPE=Release -D IRKit_BUILD_INTEGRATION_TEST:BOOL=OFF -D IRKit_BUILD_BENCHMARKS:BOOL=OFF ..
    cat CMakeFiles/CMakeError.log
    cat CMakeFiles/CMakeOutput.log
    make VERBOSE=1
    ctest -V
elif [ "$SUITE" == "integration" ]; then
    cd build
    cmake -D CMAKE_BUILD_TYPE=Release -D IRKit_BUILD_INTEGRATION_TEST:BOOL=ON -D IRKit_BUILD_BENCHMARKS:BOOL=OFF ..
    cat CMakeFiles/CMakeError.log
    cat CMakeFiles/CMakeOutput.log
    make VERBOSE=1
    ctest -V -R integration*
elif [ "$SUITE" == "benchmarks" ]; then
    cd build
    cmake -D CMAKE_BUILD_TYPE=Release -D IRKit_BUILD_INTEGRATION_TEST:BOOL=OFF -D IRKit_BUILD_BENCHMARKS:BOOL=ON ..
    cat CMakeFiles/CMakeError.log
    cat CMakeFiles/CMakeOutput.log
    make VERBOSE=1
fi
