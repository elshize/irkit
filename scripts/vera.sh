#!/bin/bash
set -x
cat \
    <(find include -type f \( -iname \*.cpp -o -iname \*.hpp \)) \
    <(find src -type f \( -iname \*.cpp -o -iname \*.hpp \)) \
    | grep -v nonstd \
    | vera++ -p irkit --exclusions profiles/exclusions --error
