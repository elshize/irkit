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
#include <CLI/Option.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include <irkit/compacttable.hpp>
#include <irkit/index.hpp>
#include <irkit/index/source.hpp>
#include <irkit/parsing/stemmer.hpp>
#include <irkit/score.hpp>
#include <irkit/taat.hpp>
#include <irkit/timer.hpp>
#include "cli.hpp"
#include "run_query.hpp"

using std::uint32_t;
using irk::index::document_t;
using mapping_t = std::vector<document_t>;
using namespace std::chrono;
using namespace irk::cli;

int main(int argc, char** argv)
{
    auto [app, args] = irk::cli::app(
        "Query inverted index",
        index_dir_opt{},
        nostem_opt{},
        id_range_opt{},
        score_function_opt{with_default<std::string>{"bm25"}},
        processing_type_opt{with_default<ProcessingType>{ProcessingType::DAAT}},
        k_opt{},
        trec_run_opt{},
        trec_id_opt{},
        terms_pos{optional});
    CLI11_PARSE(*app, argc, argv);

    boost::filesystem::path dir(args->index_dir);
    std::vector<std::string> scores;
    if (args->score_function[0] != '*') {
        scores.push_back(args->score_function);
    }
    auto data = irk::inverted_index_mapped_data_source::from(dir, {scores}).value();
    irk::inverted_index_view index(&data);

    if (not args->terms.empty()) {
        irk::run_and_print(index,
                           args->terms,
                           args->k,
                           args->score_function,
                           args->processing_type,
                           args->trec_id != -1 ? std::make_optional(args->trec_id) : std::nullopt,
                           args->trec_run);
    }
    else {
        irk::run_queries(
            app->count("--trec-id") > 0u ? std::make_optional(args->trec_id) : std::nullopt,
            [&, args = args.get()](const auto& current_trecid, const auto& terms) {
                irk::run_and_print(index,
                                   terms,
                                   args->k,
                                   args->score_function,
                                   args->processing_type,
                                   current_trecid,
                                   args->trec_run);
            });
    }
}
