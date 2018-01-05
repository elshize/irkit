set -e
mkdir -p build
cd build
cmake ../
make
make test
#for t in tests/test*; do $t; done
cd ../
