SUITE=$1

if [ "$SUITE" == "unit" ]; then
    cmake -D CMAKE_BUILD_TYPE=Release -D IRKit_BUILD_INTEGRATION_TEST:BOOL=OFF -D IRKit_BUILD_BENCHMARKS:BOOL=OFF .
    cmake --build .
    ctest -V
elif [ "$SUITE" == "integration" ]; then
    cmake -D CMAKE_BUILD_TYPE=Release -D IRKit_BUILD_INTEGRATION_TEST:BOOL=ON -D IRKit_BUILD_BENCHMARKS:BOOL=OFF .
    cmake --build .
    ctest -R -V integration*
elif [ "$SUITE" == "benchmarks" ]; then
    cmake -D CMAKE_BUILD_TYPE=Release -D IRKit_BUILD_INTEGRATION_TEST:BOOL=OFF -D IRKit_BUILD_BENCHMARKS:BOOL=ON .
    cmake --build .
    sudo make install
    ./benchmarks/all.sh
elif [ "$SUITE" == "vera" ]; then
    ./scripts/vera.sh
#elif [ "$SUITE" == "tidy" ]; then
#    ~/builds/clang-tools-extra/clang-tidy/tool/run-clang-tidy.py -header-filter=.*include/irkit/[^0-9]*hpp
fi
