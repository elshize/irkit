#!/bin/bash
cat \
    <(find include -type f \( -iname \*.cpp -o -iname \*.hpp \)) \
    <(find src -type f \( -iname \*.cpp -o -iname \*.hpp \)) \
    <(find tests -type f \( -iname \*.cpp -o -iname \*.hpp \)) \
    <(find benchmarks -type f \( -iname \*.cpp -o -iname \*.hpp \)) \
    | vera++ -p irkit --exclusions profiles/exclusions --error
