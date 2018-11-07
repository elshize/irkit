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
#include <string>
#include <unordered_set>

#include <CLI/CLI.hpp>
#include <boost/filesystem.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <tbb/task_scheduler_init.h>

#include <irkit/index.hpp>
#include <irkit/index/score.hpp>
#include <irkit/index/source.hpp>
#include <irkit/timer.hpp>
#include "cli.hpp"

namespace fs = boost::filesystem;
using source_type = irk::inverted_index_mapped_data_source;

using namespace irk::cli;

int main(int argc, char** argv)
{
    auto [app, args] = irk::cli::app(
        "Calculate score statistics",
        index_dir_opt{},
        threads_opt{},
        score_function_opt{with_default<std::string>{"bm25"}});
    CLI11_PARSE(*app, argc, argv);

    tbb::task_scheduler_init init(args->threads);
    auto log = spdlog::stderr_color_mt("console");
    log->info("Calculating score statistics using {} threads", args->threads);
    auto calc_bm25 = irk::index::calc_score_stats<
        irk::score::bm25_tag,
        irk::inverted_index_view,
        source_type>;
    auto calc_ql = irk::index::calc_score_stats<
        irk::score::query_likelihood_tag,
        irk::inverted_index_view,
        source_type>;
    fs::path dir_path(args->index_dir);
    auto score_function = args->score_function;
    auto index_dir = args->index_dir;
    irk::run_with_timer<std::chrono::milliseconds>(
        [&]() {
            if (score_function == "bm25") {
                if (auto res = calc_bm25(index_dir); not res) {
                    log->error("Fatal error: {}", res.error());
                }
            } else if (score_function == "ql") {
                if (auto res = calc_ql(index_dir); not res) {
                    log->error("Fatal error: {}", res.error());
                }
            }
        },
        irk::cli::log_finished{log});
    return 0;
}
