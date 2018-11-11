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

#pragma once

#include <algorithm>

#include <gsl/span>

#include <irkit/algorithm/group_by.hpp>
#include <irkit/algorithm/transform.hpp>
#include <irkit/score.hpp>
#include <irkit/utils.hpp>

namespace irk {

namespace detail {

    template<typename RngRng, typename ScorerRng, typename Index>
    auto daat(
        const RngRng& postings,
        int k,
        const ScorerRng& scorers,
        const Index& index)
    {
        using document_type =
            std::decay_t<decltype(postings.begin()->begin()->document())>;
        using score_type = double;
        std::vector<ScorerRng> score_fns;
        //return daat(gsl::make_span(postings), gsl::make_span(score_fns), k);
        irk::top_k_accumulator<document_type, score_type> acc(k);
        auto merged = merge(postings);
        double score{0};
        auto pos = merged.begin();
        auto last = merged.end();
        while (pos != last) {
            auto current = pos->document();
            auto doc_size = index.document_size(current);
            std::tie(score, pos) = accumulate_while(
                pos,
                last,
                double{0.0},
                [current](const auto& post) {
                    return post.document() == current;
                },
                [&](const auto& acc, const auto& posting) {
                    return acc
                        + scorers[posting.term_id()](
                               posting.payload(), doc_size);
                });
            acc.accumulate(current, score);
        }
        return acc.sorted();
    }

}  // namespace detail

//template<typename RngRng>
//auto daat(const RngRng& postings, int k)
//{
//    using document_type =
//        std::decay_t<decltype(postings.begin()->begin()->document())>;
//    using score_type =
//        std::decay_t<decltype(postings.begin()->begin()->payload())>;
//    irk::top_k_accumulator<document_type, score_type> acc(k);
//    auto merged = merge(postings);
//    group_by(
//        merged.begin(),
//        merged.end(),
//        [](const auto& p) { return p.document(); })
//        .aggregate_groups(
//            [](const auto& acc, const auto& posting) {
//                return acc + posting.payload();
//            },
//            score_type(0))
//        .for_each([&acc](const auto& id, const auto& score) {
//            acc.accumulate(id, score);
//        });
//    return acc.sorted();
//}

template<typename RngRng, typename Index>
auto daat(const RngRng& postings, int k, const Index& index, score::bm25_tag)
{
    std::vector<score::BM25TermScorer<Index>> scorers;
    for (const auto& posting_list : postings) {
        scorers.push_back(
            index.term_scorer(posting_list.term_id(), score::bm25));
    }
    return irk::daat(
        gsl::make_span(postings),
        gsl::span<const score::BM25TermScorer<Index>>(scorers),
        k);
}

template<typename RngRng, typename Index>
auto daat(
    const RngRng& postings,
    int k,
    const Index& index,
    score::query_likelihood_tag)
{
    std::vector<score::QueryLikelihoodTermScorer<Index>> scorers;
    for (const auto& posting_list : postings) {
        scorers.push_back(
            index.term_scorer(posting_list.term_id(), score::query_likelihood));
    }
    return irk::daat(
        gsl::make_span(postings),
        gsl::span<const score::QueryLikelihoodTermScorer<Index>>(scorers),
        k);
}

}  // namespace irk
