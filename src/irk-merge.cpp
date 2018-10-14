// MIT License
//
// Copyright (c) 2018 Michal Siedlaczek
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

//! \file
//! \author     Michal Siedlaczek
//! \copyright  MIT License

#include <iostream>

#include <boost/filesystem.hpp>

#include "irkit/index.hpp"
#include "irkit/index/merger.hpp"

namespace fs = boost::filesystem;

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cerr << "usage: irk-merge <target_index_dir> [<parts>...]\n";
        exit(1);
    }

    fs::path target_dir(argv[1]);
    if (not fs::exists(target_dir)) {
        fs::create_directory(target_dir);
    }
    std::vector<fs::path> parts;
    for (int arg = 2; arg < argc; ++arg) {
        parts.emplace_back(argv[arg]);
    }

    irk::default_index_merger merger(target_dir, parts, true);
    std::cout << "Merging titles... ";
    std::cout.flush();
    merger.merge_titles();
    std::cout << "done.\nMerging terms... ";
    std::cout.flush();
    merger.merge_terms();
    std::cout << "done.\n";

    return 0;
}
