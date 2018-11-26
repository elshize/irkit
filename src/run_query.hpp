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
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <chrono>
#include <iostream>

#include <CLI/CLI.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <cppitertools/itertools.hpp>
#include <fmt/format.h>

#include <irkit/algorithm/query.hpp>
#include <irkit/index.hpp>
#include <irkit/index/cluster.hpp>
#include <irkit/index/source.hpp>
#include <irkit/score.hpp>
#include <irkit/timer.hpp>

#include "cli.hpp"

namespace irk {

template<class ScoreTag, class Index, class Rng>
auto fetch_scorers(const Index& index, const Rng& terms)
{
    std::vector<decltype(index.term_scorer(0, ScoreTag{}))> scorers;
    for (const auto& term : terms) {
        if (auto term_id = index.term_id(term); term_id) {
            scorers.push_back(index.term_scorer(term_id.value(), ScoreTag{}));
        } else {
            scorers.push_back(index.term_scorer(0, ScoreTag{}));
        }
    }
    return scorers;
}

template<class ScoreTag, class IndexCluster, class Index, class Rng>
auto fetch_global_scorers(const IndexCluster& index_cluster,
                          const Index& shard_index,
                          const Rng& terms)
{
    using scorer_type = decltype(
        index_cluster.term_scorer(shard_index, 0, ScoreTag{}));
    std::vector<scorer_type> scorers;
    for (const auto& term : terms) {
        if (auto term_id = index_cluster.term_id(term); term_id) {
            scorers.push_back(index_cluster.term_scorer(
                shard_index, term_id.value(), ScoreTag{}));
        } else {
            scorers.push_back(
                index_cluster.term_scorer(shard_index, 0, ScoreTag{}));
        }
    }
    return scorers;
}

template<class ScoreTag, class Index, class StrRng>
inline auto run_query_with_scoring(const Index& index,
                                   const StrRng& query,
                                   const int k,
                                   cli::ProcessingType proctype)
{
    const auto scorers = fetch_scorers<ScoreTag>(index, query);
    const auto postings = irk::fetched_query_postings(index, query);
    assert(scorers.size() == postings.size());
    switch (proctype) {
    case cli::ProcessingType::TAAT:
        return irk::taat(gsl::make_span(postings),
                         gsl::make_span(scorers),
                         index.collection_size(),
                         k);
    case cli::ProcessingType::DAAT:
        return irk::daat(gsl::make_span(postings), gsl::make_span(scorers), k);
    }
    throw std::runtime_error("non-exhaustive switch");
}

template<class Index, class StrRng>
inline auto run_query_with_precomputed(const Index& index,
                                       const StrRng& query,
                                       const int k,
                                       cli::ProcessingType proctype)
{
    switch (proctype) {
    case cli::ProcessingType::TAAT:
        return irk::taat(
            gsl::make_span(irk::fetched_query_scored_postings(index, query)),
            index.collection_size(),
            k);
    case cli::ProcessingType::DAAT:
        return irk::daat(
            gsl::make_span(irk::fetched_query_scored_postings(index, query)),
            k);
    }
    throw std::runtime_error("non-exhaustive switch");
}

template<bool on_fly, class Index, class StrRng>
inline auto run_query(const Index& index,
                      const StrRng& query,
                      const int k,
                      const std::string scorer,
                      cli::ProcessingType proctype)
{
    if constexpr (on_fly) {
        if (scorer == "*bm25") {
            return run_query_with_scoring<score::bm25_tag>(
                index, query, k, proctype);
        } else {
            return run_query_with_scoring<score::query_likelihood_tag>(
                index, query, k, proctype);
        }
    } else {
        return run_query_with_precomputed(index, query, k, proctype);
    }
}

template<class Index, class Score>
inline void
print_results(const std::vector<std::pair<index::document_t, Score>>& results,
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

template<class Score>
inline void
print_results(const std::vector<std::pair<std::string, Score>>& results,
              std::optional<int> trecid,
              std::string_view run_id)
{
    int rank = 0;
    for (auto& result : results)
    {
        if (trecid.has_value()) {
            std::cout << *trecid << '\t' << "Q0\t" << result.first << "\t"
                      << rank++ << "\t" << result.second << "\t" << run_id
                      << "\n";
        }
        else {
            std::cout << result.first << "\t" << result.second << '\n';
        }
    }
}

template<class Index, class StrRng>
inline auto run_and_print(const Index& index,
                          const StrRng& query,
                          const int k,
                          const std::string scorer,
                          irk::cli::ProcessingType proctype,
                          std::optional<int> trecid,
                          std::string_view run_id)
{
    if (irk::cli::on_fly(scorer)) {
        auto results = irk::run_query<true>(index, query, k, scorer, proctype);
        print_results(results, index, trecid, run_id);
    } else {
        auto results = irk::run_query<false>(index, query, k, scorer, proctype);
        print_results(results, index, trecid, run_id);
    }
}

template<class StrRng, class ResultVec, class ShardIndex, class Scorer>
void rescore(ResultVec& results,
             const ShardIndex& shard_index,
             const StrRng& query,
             const std::vector<Scorer>& scorers)
{
    EXPECTS(query.size() == scorers.size());
    std::sort(std::begin(results), std::end(results), [](const auto& lhs, const auto& rhs) {
        return lhs.first < rhs.first;
    });
    std::for_each(
        std::begin(results), std::end(results), [](auto&& result) { result.second = 0.0; });
    for (const auto& [term, scorer] : iter::zip(query, scorers)) {
        auto term_postings = shard_index.postings(term);
        auto pos = term_postings.begin();
        auto end = term_postings.end();
        for (auto&& result : results) {
            pos.advance_to(result.first);
            if (pos == end) {
                break;
            }
            if (pos->document() == result.first) {
                result.second += scorer(pos->document(), pos->payload());
            }
        }
    }
}

template<class ScoreTag, class IndexCluster, class StrRng>
inline auto run_shards(const IndexCluster& index,
                       const StrRng& query,
                       const int k,
                       const std::string scorer,
                       irk::cli::ProcessingType proctype,
                       std::optional<int> trecid,
                       std::string_view run_id,
                       ScoreTag score_tag)
{
    using Document = decltype(
        irk::run_query<true>(
            index.shard(std::declval<ShardId>()), query, k, scorer, proctype)[0]
            .first);
    irk::top_k_accumulator<std::string, double> acc(k);
    for (auto shard_id : ShardId::range(index.shard_count())) {
        const auto& shard_index = index.shard(shard_id);
        auto global_scorers = fetch_global_scorers<ScoreTag>(
            index, shard_index, query);
        auto results = irk::run_query<true>(
            shard_index, query, k, scorer, proctype);
        rescore(results, shard_index, query, global_scorers);
        const auto& titles = shard_index.titles();
        for (auto&& [doc, score] : results) {
            auto title = titles.key_at(doc);
            acc.accumulate(title, score);
        }
    }
    print_results(acc.sorted(), trecid, run_id);
}

template<class IndexCluster, class StrRng>
inline auto run_shards(const IndexCluster& index,
                       const StrRng& query,
                       const int k,
                       const std::string scorer,
                       irk::cli::ProcessingType proctype,
                       std::optional<int> trecid,
                       std::string_view run_id)
{
    using Score = typename IndexCluster::score_type;
    irk::top_k_accumulator<std::string, Score> acc(k);
    for (const auto& shard_index : index.shards()) {
        auto results       = irk::run_query<false>(shard_index, query, k, scorer, proctype);
        const auto& titles = shard_index.titles();
        for (auto&& [doc, score] : results) {
            auto title = titles.key_at(doc);
            acc.accumulate(title, score);
        }
    }
    print_results(acc.sorted(), trecid, run_id);
}

template<class IndexCluster, class StrRng>
inline void run_shards(const bool on_fly,
                       const IndexCluster& index,
                       const StrRng& query,
                       const int k,
                       const std::string scorer,
                       irk::cli::ProcessingType proctype,
                       std::optional<int> trecid,
                       std::string_view run_id)
{
    if (on_fly) {
        if (scorer == "*bm25") {
            irk::run_shards(index, query, k, scorer, proctype, trecid, run_id, score::bm25);
        } else {
            irk::run_shards(
                index, query, k, scorer, proctype, trecid, run_id, score::query_likelihood);
        }
    } else {
        irk::run_shards(index, query, k, scorer, proctype, trecid, run_id);
    }
}

template<class Fn>
inline void run_queries(std::optional<int> current_trecid, Fn run)
{
    for (const auto& query_line : irk::io::lines_from_stream(std::cin)) {
        std::vector<std::string> terms;
        boost::split(terms, query_line, boost::is_any_of("\t "), boost::token_compress_on);
        run(current_trecid, terms);
        if (current_trecid.has_value()) {
            current_trecid.value()++;
        }
    }
}

}  // namespace irk
