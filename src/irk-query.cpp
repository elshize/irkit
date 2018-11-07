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

using std::uint32_t;
using irk::index::document_t;
using mapping_t = std::vector<document_t>;
using namespace std::chrono;
using namespace irk::cli;

template<class Index, class Score>
inline void print_results(
    const std::vector<std::pair<document_t, Score>>& results,
    const Index& index,
    std::optional<int> trecid,
    std::string_view run_id)
{
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
}

inline void stem_if(std::vector<std::string>& query, bool stem)
{
    if (stem) {
        irk::porter2_stemmer stemmer;
        for (auto& term : query) {
            term = stemmer.stem(term);
        }
    }
}

template<class Index>
inline void run_and_score(
    const Index& index,
    const boost::filesystem::path& dir,
    std::vector<std::string>& query,
    int k,
    bool stem,
    std::optional<int> trecid,
    std::string_view run_id,
    const std::string scorer,
    ProcessingType proctype)
{
    stem_if(query, stem);
    auto total = irk::run_with_timer<std::chrono::milliseconds>([&]() {
        auto postings = postings_on_fly(query, index, scorer);
        auto results = process_query(index, postings, k, proctype);
        print_results<Index, double>(results, index, trecid, run_id);
    });
    std::cerr << "Time: " << total.count() << " ms\n";
}

template<class Index>
inline void run_query(const Index& index,
    const boost::filesystem::path& dir,
    std::vector<std::string>& query,
    int k,
    bool stem,
    std::optional<int> trecid,
    std::string_view run_id)
{
    stem_if(query, stem);
    auto start_time = steady_clock::now();

    auto [postings, fetch_time] = irk::run_with_timer_ret<milliseconds>(
        [&]() { return irk::query_scored_postings(index, query); });
    auto [acc, init_time] = irk::run_with_timer_ret<milliseconds>(
        [&]() { return std::vector<uint32_t>(index.collection_size(), 0); });
    auto acc_time =
        irk::run_with_timer<milliseconds>([&]() { irk::taat(postings, acc); });
    auto [results, agg_time] = irk::run_with_timer_ret<milliseconds>([&]() {
        return irk::aggregate_top_k<document_t, uint32_t>(
            std::begin(acc), std::end(acc), k);
    });
    auto end_time = steady_clock::now();
    auto total = duration_cast<milliseconds>(end_time - start_time);

    print_results(results, index, trecid, run_id);
    std::cerr << "Time: " << total.count() << " ms [ ";
    std::cerr << "Fetch: " << fetch_time.count() << " / ";
    std::cerr << "Init: " << init_time.count() << " / ";
    std::cerr << "Acc: " << acc_time.count() << " / ";
    std::cerr << "Agg: " << agg_time.count() << " ]\n";
}

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
    auto data =
        irk::inverted_index_mapped_data_source::from(dir, {scores}).value();
    irk::inverted_index_view index(&data);

    if (not args->terms.empty()) {
        if (on_fly(args->score_function)) {
            run_and_score(
                index,
                args->index_dir,
                args->terms,
                args->k,
                not args->nostem,
                args->trec_id != -1 ? std::make_optional(args->trec_id)
                                    : std::nullopt,
                args->trec_run,
                args->score_function,
                args->processing_type);
            return 0;
        }
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
            if (on_fly(args->score_function)) {
                run_and_score(
                    index,
                    args->index_dir,
                    terms,
                    args->k,
                    not args->nostem,
                    current_trecid,
                    args->trec_run,
                    args->score_function,
                    args->processing_type);
            } else {
                run_query(
                    index,
                    args->index_dir,
                    terms,
                    args->k,
                    not args->nostem,
                    current_trecid,
                    args->trec_run);
            }
            if (current_trecid.has_value()) {
                current_trecid.value()++;
            }
        }
    }
}
