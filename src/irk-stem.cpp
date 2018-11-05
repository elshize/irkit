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

#include <CLI/CLI.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <cppitertools/itertools.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <taily.hpp>

#include <irkit/algorithm.hpp>
#include <irkit/index.hpp>
#include <irkit/index/source.hpp>
#include <irkit/parsing/stemmer.hpp>
#include <irkit/threshold.hpp>
#include <irkit/utils.hpp>
#include "cli.hpp"

using boost::filesystem::path;
using irk::index::document_t;
using std::uint32_t;
using namespace irk;

int main(int argc, char** argv)
{
    auto [app, args] =
        irk::cli::app("Stem tokens", irk::cli::terms_pos{irk::cli::optional});
    CLI11_PARSE(*app, argc, argv);

    irk::porter2_stemmer stem{};
    if (not args->terms.empty()) {
        for (const std::string& term : args->terms) {
            std::cout << stem(term) << '\n';
        }
    }
    else {
        std::string term;
        while (std::cin >> term) {
            std::cout << stem(term) << '\n';
        }
    }
    return 0;
}
