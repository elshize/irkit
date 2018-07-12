#!/bin/bash
cmake -D CMAKE_EXPORT_COMPILE_COMMANDS=ON -D IRKit_BUILD_TEST=OFF ..
python ../scripts/remove-submodules.py
run-clang-tidy.py -header-filter=.*include/irkit/[^0-9]*hpp
