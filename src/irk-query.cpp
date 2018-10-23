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

#include "cli.hpp"
#include <irkit/compacttable.hpp>
#include <irkit/index.hpp>
#include <irkit/index/source.hpp>
#include <irkit/parsing/stemmer.hpp>
#include <irkit/taat.hpp>

using std::uint32_t;
using irk::index::document_t;
using mapping_t = std::vector<document_t>;

template<class Index>
inline void run_query(const Index& index,
    const boost::filesystem::path& dir,
    std::vector<std::string>& query,
    int k,
    bool stem,
    std::optional<int> trecid,
    std::string_view run_id)
{
    if (stem) {
        irk::porter2_stemmer stemmer;
        for (auto& term : query) {
            term = stemmer.stem(term);
        }
    }

    auto start_time = std::chrono::steady_clock::now();

    auto postings = irk::query_scored_postings(index, query);
    auto after_fetch = std::chrono::steady_clock::now();

    std::vector<uint32_t> acc(index.collection_size(), 0);
    auto after_init = std::chrono::steady_clock::now();

    irk::taat(postings, acc);

    auto after_acc = std::chrono::steady_clock::now();

    auto results = irk::aggregate_top_k<document_t, uint32_t>(
        std::begin(acc), std::end(acc), k);
    auto end_time = std::chrono::steady_clock::now();

    auto total = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    auto fetch = std::chrono::duration_cast<std::chrono::milliseconds>(
        after_fetch - start_time);
    auto init = std::chrono::duration_cast<std::chrono::milliseconds>(
        after_init - after_fetch);
    auto accum = std::chrono::duration_cast<std::chrono::milliseconds>(
        after_acc - after_init);
    auto agg = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - after_acc);

    const auto& titles = index.titles();
    int rank = 0;
    for (auto& result : results)
    {
        std::string title = titles.key_at(result.first);
        if (trecid.has_value()) {
            std::cout << *trecid << '\t'
                      << "Q0\t"
                      << title << "\t"
                      << rank++ << "\t"
                      << result.second << "\t"
                      << run_id << "\n";
        }
        else {
            std::cout << title << "\t" << result.second << '\n';
        }
    }
    std::cerr << "Time: " << total.count() << " ms [ ";
    std::cerr << "Fetch: " << fetch.count() << " / ";
    std::cerr << "Init: " << init.count() << " / ";
    std::cerr << "Acc: " << accum.count() << " / ";
    std::cerr << "Agg: " << agg.count() << " ]\n";
}

int main(int argc, char** argv)
{
    auto [app, args] = irk::cli::app(
        "Query inverted index",
        irk::cli::index_dir_opt{},
        irk::cli::nostem_opt{},
        irk::cli::id_range_opt{},
        irk::cli::k_opt{},
        irk::cli::score_function_opt{
            irk::cli::with_default<std::string>{"bm25"}},
        irk::cli::trec_run_opt{},
        irk::cli::trec_id_opt{},
        irk::cli::terms_pos{irk::cli::optional});
    CLI11_PARSE(*app, argc, argv);

    boost::filesystem::path dir(args->index_dir);
    auto data = irk::inverted_index_mapped_data_source::from(
                    dir, {args->score_function})
                    .value();
    irk::inverted_index_view index(&data);

    if (not args->terms.empty()) {
        run_query(
            index,
            args->index_dir,
            args->terms,
            args->k,
            not args->nostem,
            args->trec_id != -1 ? std::make_optional(args->trec_id)
                                : std::nullopt,
            args->trec_run);
    }
    else {
        std::optional<int> current_trecid = app->count("--trec-id") > 0u
            ? std::make_optional(args->trec_id)
            : std::nullopt;
        for (const auto& query_line : irk::io::lines_from_stream(std::cin)) {
            std::vector<std::string> terms;
            boost::split(
                terms,
                query_line,
                boost::is_any_of("\t "),
                boost::token_compress_on);
            run_query(
                index,
                args->index_dir,
                terms,
                args->k,
                not args->nostem,
                current_trecid,
                args->trec_run);
            if (current_trecid.has_value()) {
                current_trecid.value()++;
            }
        }
    }
}
