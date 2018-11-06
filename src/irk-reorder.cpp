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

#include <chrono>
#include <iostream>

#include <CLI/CLI.hpp>
#include <boost/filesystem.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <irkit/index.hpp>
#include <irkit/index/reorder.hpp>
#include <irkit/index/source.hpp>
#include <irkit/timer.hpp>
#include "cli.hpp"

using boost::filesystem::path;
using irk::index::document_t;
using std::uint32_t;
using namespace irk;

std::vector<document_t>
compute_permutation(std::istream& in, const path& input_dir)
{
    std::vector<document_t> permutation;
    auto source = inverted_index_mapped_data_source::from(input_dir).value();
    inverted_index_view index(&source);
    permutation.reserve(index.collection_size());
    const auto& titles = index.titles();
    std::string title;
    while (std::getline(in, title)) {
        if (auto id = titles.index_at(title); id) {
            permutation.push_back(id.value());
        }
    }
    return permutation;
}

int main(int argc, char** argv)
{
    std::string spam_file;
    std::string ordering_file = "";
    std::string output_dir;
    auto [app, args] = irk::cli::app(
        "Build an index with reordered documents.", cli::index_dir_opt{});
    app->add_option(
        "--ordering",
        ordering_file,
        "New document reordering (titles). "
        "Absent documents will be removed from the output index. "
        "When this option is not defined, the titles will be read from stdin.",
        false);
    app->add_option("output-dir", output_dir, "Output index directory", false)
        ->required()
        ->check(cli::ExistingDirectory);
    CLI11_PARSE(*app, argc, argv);

    auto log = spdlog::stderr_color_mt("stderr");
    path dir(args->index_dir);
    std::vector<document_t> permutation;
    irk::run_with_timer<std::chrono::milliseconds>(
        [&]() {
            log->info("Computing permutation...");
            if (ordering_file.empty()) {
                permutation = compute_permutation(std::cin, dir);
            } else {
                std::ifstream in(ordering_file);
                permutation = compute_permutation(in, dir);
            }
        },
        irk::cli::log_finished{log});
    irk::run_with_timer<std::chrono::milliseconds>(
        [&]() {
            log->info("Reordering...");
            irk::reorder::index(dir, path(output_dir), permutation);
        },
        irk::cli::log_finished{log});
    return 0;
}
