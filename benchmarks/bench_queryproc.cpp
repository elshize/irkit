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
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/min.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <cppitertools/itertools.hpp>
#include <fmt/format.h>

#include <irkit/algorithm/query.hpp>
#include <irkit/index.hpp>
#include <irkit/index/source.hpp>
#include <irkit/score.hpp>
#include <irkit/timer.hpp>
#include "../src/cli.hpp"

using namespace irk::cli;
using boost::accumulators::accumulator_set;
using boost::accumulators::stats;
using boost::accumulators::tag::max;
using boost::accumulators::tag::mean;
using boost::accumulators::tag::min;
using boost::accumulators::tag::variance;

template<class Index, class RngRng>
auto scorers(const Index& index, const RngRng& postings, irk::score::bm25_tag)
{
    std::vector<irk::score::BM25TermScorer<Index>> scorers;
    for (const auto& posting_list : postings) {
        scorers.push_back(
            index.term_scorer(posting_list.term_id(), irk::score::bm25));
    }
    return scorers;
}

template<class Index, class RngRng>
auto scorers(
    const Index& index,
    const RngRng& postings,
    irk::score::query_likelihood_tag)
{
    std::vector<irk::score::QueryLikelihoodTermScorer<Index>> scorers;
    for (const auto& posting_list : postings) {
        scorers.push_back(index.term_scorer(
            posting_list.term_id(), irk::score::query_likelihood));
    }
    return scorers;
}

template<class Index>
inline auto run_query(
    const Index& index,
    const boost::filesystem::path& dir,
    std::vector<std::string>& query,
    int k,
    const std::string scorer,
    ProcessingType proctype)
{
    if (on_fly(scorer)) {
        if (proctype == ProcessingType::TAAT) {
            return irk::run_with_timer<std::chrono::nanoseconds>([&]() {
                const auto postings = irk::fetched_query_postings(index, query);
                if (scorer == "*bm25") {
                    const auto bm25_scorers =
                        scorers(index, postings, irk::score::bm25);
                    irk::taat(
                        gsl::make_span(postings),
                        gsl::make_span(bm25_scorers),
                        index.collection_size(),
                        k);
                } else {
                    const auto ql_scorers =
                        scorers(index, postings, irk::score::query_likelihood);
                    irk::taat(
                        gsl::make_span(postings),
                        gsl::make_span(ql_scorers),
                        index.collection_size(),
                        k);
                }
            });
        }
        return irk::run_with_timer<std::chrono::nanoseconds>([&]() {
            const auto postings = irk::fetched_query_postings(index, query);
            if (scorer == "*bm25") {
                irk::daat(postings, k, index, irk::score::bm25);
            } else {
                irk::daat(postings, k, index, irk::score::query_likelihood);
            }
        });
    } else {
        return irk::run_with_timer<std::chrono::nanoseconds>([&]() {
            auto postings = irk::query_scored_postings(index, query);
            process_query(index, postings, k, proctype);
        });
    }
}

int main(int argc, char** argv)
{
    int repeat = 10;
    auto [app, args] = irk::cli::app(
        "Query processing benchmark",
        index_dir_opt{},
        nostem_opt{},
        sep_opt{},
        score_function_opt{with_default<std::string>{"bm25"}},
        processing_type_opt{with_default<ProcessingType>{ProcessingType::DAAT}},
        k_opt{});
    CLI11_PARSE(*app, argc, argv);

    boost::filesystem::path dir(args->index_dir);
    std::vector<std::string> scores;
    if (args->score_function[0] != '*') {
        scores.push_back(args->score_function);
    }
    auto data = irk::inverted_index_mapped_data_source::from(dir, scores);
    if (not data) {
        std::cerr << data.error() << '\n';
        return 1;
    }
    irk::inverted_index_view index(&data.value());

    for (const auto& query_line : irk::io::lines_from_stream(std::cin)) {
        std::vector<std::string> terms;
        boost::split(
            terms,
            query_line,
            boost::is_any_of("\t "),
            boost::token_compress_on);
        irk::cli::stem_if(not args->nostem, terms);
        accumulator_set<float, stats<mean, min, max>> acc;
        for ([[maybe_unused]] auto iteration : iter::range(repeat)) {
            auto time = run_query(
                index,
                args->index_dir,
                terms,
                args->k,
                args->score_function,
                args->processing_type);
            acc(static_cast<int64_t>(time.count()));
        }
        std::cout << fmt::format(
            "{1}{0}{2}{0}{3}{0}{4}\n",
            args->separator,
            query_line,
            boost::accumulators::min(acc) / 1000000,
            boost::accumulators::max(acc) / 1000000,
            boost::accumulators::mean(acc) / 1000000);
    }
}
