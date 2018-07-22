#!/bin/bash
cmake -D IRKit_USE_SYSTEM_BOOST=ON -D CMAKE_EXPORT_COMPILE_COMMANDS=ON -D IRKit_BUILD_TEST=OFF ..
run-clang-tidy.py -header-filter=.*include/irkit/[^0-9]*hpp
