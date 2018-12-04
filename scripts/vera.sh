#!/bin/bash
set -x
cat \
    <(find include -type f \( -iname \*.cpp -o -iname \*.hpp \)) \
    <(find src -type f \( -iname \*.cpp -o -iname \*.hpp \)) \
    | grep -v nonstd \
    | grep -v boost \
    | vera++ -p irkit --exclusions profiles/exclusions --error
