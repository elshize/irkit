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

#include <irkit/algorithm/query.hpp>
#include <irkit/compacttable.hpp>
#include <irkit/index.hpp>
#include <irkit/index/source.hpp>
#include <irkit/parsing/stemmer.hpp>
#include <irkit/query_engine.hpp>
#include <irkit/score.hpp>
#include <irkit/taat.hpp>
#include <irkit/timer.hpp>
#include "cli.hpp"
#include "run_query.hpp"

using irk::Query_Engine;
using irk::Traversal_Type;
using irk::index::document_t;
using std::uint32_t;
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
        traversal_type_opt{with_default<Traversal_Type>{Traversal_Type::DAAT}},
        k_opt{},
        trec_run_opt{},
        trec_id_opt{},
        terms_pos{optional});
    CLI11_PARSE(*app, argc, argv);

    boost::filesystem::path dir(args->index_dir);
    std::vector<std::string> scores;
    if (Query_Engine::is_quantized(args->score_function)) {
        scores.push_back(args->score_function);
    }
    auto data = irk::Inverted_Index_Mapped_Source::from(dir, {scores});
    irk::inverted_index_view index(irtl::value(data));
    auto const& titles = index.titles();
    auto engine = Query_Engine::from(
        index,
        args->nostem,
        args->score_function,
        args->traversal_type,
        app->count("--trec-id") > 0u ? std::make_optional(args->trec_id) : std::optional<int>{},
        args->trec_run);
    if (not args->terms.empty())
    {
        engine.run_query(args->terms, args->k).print([&](int rank, auto document, auto score) {
            std::string title = titles.key_at(document);
            std::cout << title << "\t" << score << '\n';
        });
    }
    else {
        std::optional<int> trec_id = app->count("--trec-id") > 0u
            ? std::make_optional(args->trec_id)
            : std::nullopt;
        irk::for_each_query(std::cin, not args->nostem, [&, k = args->k](auto id, auto terms) {
            engine.run_query(terms, k).print([&, run_id = args->trec_run](
                                                 int rank, auto document, auto score) {
                std::string title = titles.key_at(document);
                if (trec_id.has_value()) {
                    std::cout << (*trec_id + id) << '\t' << "Q0\t" << title << "\t" << rank << "\t"
                              << score << "\t" << run_id << "\n";
                } else {
                    std::cout << title << "\t" << score << '\n';
                }
            });
        });
    }
}
