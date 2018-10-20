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

void threshold(
    const irk::inverted_index_view& index,
    std::vector<std::string>& terms,
    int topk,
    bool nostem)
{
    if (not nostem) {
        irk::inplace_transform(
            terms.begin(), terms.end(), irk::porter2_stemmer{});
    }

    auto documents = irk::query_documents(index, terms);
    auto scores = irk::query_scores(index, terms);
    auto threshold = irk::compute_threshold(
        documents.begin(), documents.end(), scores.begin(), scores.end(), topk);
    std::cout << threshold << '\n';
}

int main(int argc, char** argv)
{
    std::string estimate_method;
    auto [app, args] = irk::cli::app(
        "Compute or estimate top-k threshold",
        irk::cli::index_dir_opt{},
        irk::cli::nostem_opt{},
        // irk::cli::id_range_opt{},
        irk::cli::k_opt{},
        irk::cli::terms_pos{irk::cli::optional});
    app->add_option(
        "-e,--estimate",
        estimate_method,
        "Method to estimate threshold. By default, it will be computed "
        "exactly.",
        false);
    CLI11_PARSE(*app, argc, argv);

    boost::filesystem::path dir(args->index_dir);
    auto data =
        irk::inverted_index_mapped_data_source::from(dir, {"bm25"}).value();
    irk::inverted_index_view index(&data);

    if (not args->terms.empty()) {
        threshold(index, args->terms, args->k, args->nostem);
    }
    else {
        for (const auto& query_line : irk::io::lines_from_stream(std::cin)) {
            std::vector<std::string> terms;
            boost::split(
                terms,
                query_line,
                boost::is_any_of("\t "),
                boost::token_compress_on);
            threshold(index, terms, args->k, args->nostem);
        }
    }
    return 0;
}
