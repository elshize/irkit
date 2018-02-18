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

//! \file taat.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <boost/concept/assert.hpp>
#include <boost/concept_check.hpp>
#include <gsl/gsl_assert>
#include <vector>
#include "irkit/concepts.hpp"
#include "irkit/types.hpp"
#include "irkit/utils.hpp"

namespace irk {

//! A simple score accumulator that adds all scores.
class simple_accumulator {
public:
    template<class Doc, class Score, class AccumulatorArray>
    void accumulate_posting(Doc doc, Score score_delta, AccumulatorArray& acc)
    {
        acc[doc] += score_delta;
    }
};
simple_accumulator default_accumulator;

//! A dummy weight type that does not change the score when multiplied.
struct no_weight {
};

template<class Score>
Score operator*=(Score& lhs, const no_weight& rhs) {
    return lhs;
}

//! Traverses and accumulates scores in a single posting list.
/*!
    TODO:
        - Document
        - Define the rest of the concepts
 */
template<class PostingRange,
    class AccumulatorArray,
    class Weight = no_weight,
    class AccumulatorPolicy = simple_accumulator>
void traverse_list_postings(const PostingRange& postings,
    AccumulatorArray& acc,
    Weight weight = no_weight{},
    AccumulatorPolicy accumulator_policy = default_accumulator)
{
    BOOST_CONCEPT_ASSERT((concept::InputRange<PostingRange>));
    for (auto[doc, score] : postings) {
        score *= weight;
        accumulator_policy.accumulate_posting(doc, score, acc);
    }
}

//! Traverses and accumulates scores in a single posting list.
template<class DocRange,
    class ScoreRange,
    class AccumulatorArray,
    class Weight = no_weight,
    class AccumulatorPolicy = simple_accumulator>
void traverse_list(const DocRange& docs,
    const ScoreRange& scores,
    AccumulatorArray& acc,
    Weight weight = no_weight{},
    AccumulatorPolicy& accumulator_policy = default_accumulator)
{
    BOOST_CONCEPT_ASSERT((concept::InputRange<DocRange>));
    BOOST_CONCEPT_ASSERT((concept::InputRange<ScoreRange>));
    using Doc = pure_element_t<DocRange>;
    using Score = pure_element_t<ScoreRange>;
    using Posting = _posting<Doc, Score>;
    traverse_list_postings(view::posting_zip<Posting>(docs, scores),
        acc,
        weight,
        accumulator_policy);
}

//! Traverses postings and accumulates scores.
template<class DocRange,
    class ScoreRange,
    class AccumulatorArray,
    class AccumulatorPolicy = simple_accumulator>
void traverse(const std::vector<DocRange>& doc_ranges,
    const std::vector<ScoreRange>& score_ranges,
    AccumulatorArray& acc,
    const std::vector<pure_element_t<ScoreRange>>& term_weights,
    AccumulatorPolicy& accumulator_policy = default_accumulator)
{
    BOOST_CONCEPT_ASSERT((concept::InputRange<DocRange>));
    BOOST_CONCEPT_ASSERT((concept::InputRange<ScoreRange>));
    Expects(doc_ranges.size() == score_ranges.size());
    for (unsigned int term_id = 0; term_id < doc_ranges.size(); ++term_id) {
        traverse_list(doc_ranges[term_id],
            score_ranges[term_id],
            acc,
            term_weights[term_id],
            accumulator_policy);
    }
}

//! Traverses postings and accumulates scores.
template<class PostingRange,
    class AccumulatorArray,
    class Weight = no_weight,
    class AccumulatorPolicy = simple_accumulator>
void traverse_postings(const std::vector<PostingRange>& posting_ranges,
    AccumulatorArray& acc,
    const std::vector<score_t<pure_element_t<PostingRange>>>& term_weights,
    AccumulatorPolicy accumulator_policy = default_accumulator)
{
    BOOST_CONCEPT_ASSERT((concept::InputRange<PostingRange>));
    for (unsigned int term_id = 0; term_id < posting_ranges.size(); ++term_id) {
        traverse_list_postings(posting_ranges[term_id],
            acc,
            term_weights[term_id],
            accumulator_policy);
    }
}

template<class Result, class AccumulatorArray>
std::vector<Result> aggregate_top(std::size_t k, const AccumulatorArray& acc)
{
    static_assert(std::is_convertible<pure_element_t<AccumulatorArray>,
        score_t<Result>>::value);
    BOOST_CONCEPT_ASSERT((concept::InputRange<AccumulatorArray>));
    using Doc = doc_t<Result>;
    using Score = score_t<Result>;
    TopKAccumulator<Result> topk(k);
    Doc doc(0);
    for (const Score& score : acc) {
        topk.accumulate({doc, score});
        doc++;
    }
    return topk.sorted();
}

template<class Range>
std::vector<pure_element_t<Range>> taat(
    const std::vector<Range>& query_postings,
    std::size_t k,
    const std::vector<score_t<pure_element_t<Range>>>& weights,
    std::size_t collection_size)
{
    using Posting = pure_element_t<Range>;
    using Score = score_t<Posting>;
    std::vector<Score> acc(collection_size, Score(0));
    traverse_postings(query_postings, acc, weights);
    return aggregate_top<Posting>(k, acc);
}

};  // namespace irk

