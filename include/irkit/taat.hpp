#pragma once

#include <gsl/gsl_assert>
#include <range/v3/range_concepts.hpp>
#include <vector>
#include "irkit/types.hpp"
#include "irkit/utils.hpp"

namespace irkit {

class SimpleAccumulator {
public:
    /// Accumulates the posting that is being processed.
    template<class Doc, class Score, class AccumulatorArray>
    void accumulate_posting(Doc doc, Score score_delta, AccumulatorArray& acc)
    {
        acc[doc] += score_delta;
    }
};
SimpleAccumulator default_accumulator;

struct IdWeight {
};

template<class Score>
Score operator*=(Score& lhs, const IdWeight& rhs) {
    return lhs;
}

//! Traverses and accumulates scores in a single posting list.
//! TODO: Document
//! TODO: Define the rest of the concepts
template<class PostingRange,
    class AccumulatorArray,
    class Weight = IdWeight,
    class AccumulatorPolicy = SimpleAccumulator,
    //CONCEPT_REQUIRES_(ranges::InputRange<PostingRange>()),
    CONCEPT_REQUIRES_(ranges::RandomAccessRange<AccumulatorArray>())>
void traverse_list_postings(const PostingRange& postings,
    AccumulatorArray& acc,
    Weight weight = IdWeight{},
    AccumulatorPolicy& accumulator_policy = default_accumulator)
{
    for (auto[doc, score] : postings) {
        score *= weight;
        accumulator_policy.accumulate_posting(doc, score, acc);
    }
}

//! Traverses and accumulates scores in a single posting list.
template<class DocRange,
    class ScoreRange,
    class AccumulatorArray,
    class Weight = IdWeight,
    class AccumulatorPolicy = SimpleAccumulator,
    CONCEPT_REQUIRES_(ranges::InputRange<DocRange>()),
    CONCEPT_REQUIRES_(ranges::InputRange<ScoreRange>()),
    CONCEPT_REQUIRES_(ranges::RandomAccessRange<AccumulatorArray>())>
void traverse_list(const DocRange& docs,
    const ScoreRange& scores,
    AccumulatorArray& acc,
    Weight weight = IdWeight{},
    AccumulatorPolicy& accumulator_policy = default_accumulator)
{
    using Doc = pure_element_t<DocRange>;
    using Score = pure_element_t<ScoreRange>;
    using Posting = _Posting<Doc, Score>;
    traverse_list_postings(view::posting_zip<Posting>(docs, scores),
        acc,
        weight,
        accumulator_policy);
    //using Doc = pure_element_t<DocRange>;
    //using Score = pure_element_t<ScoreRange>;
    //auto docit = std::cbegin(docs);
    //auto scoreit = std::cbegin(scores);
    //for (; docit < std::cend(docs); ++docit, ++scoreit) {
    //    Doc doc = *docit;
    //    Score score = *scoreit;
    //    score *= weight;
    //    accumulator_policy.accumulate_posting(doc, score, acc);
    //}
}

//! Traverses postings and accumulates scores.
template<class DocRange,
    class ScoreRange,
    class AccumulatorArray,
    class AccumulatorPolicy = SimpleAccumulator,
    CONCEPT_REQUIRES_(ranges::InputRange<DocRange>()),
    CONCEPT_REQUIRES_(ranges::InputRange<ScoreRange>()),
    CONCEPT_REQUIRES_(ranges::RandomAccessRange<AccumulatorArray>())>
void traverse(const std::vector<DocRange>& doc_ranges,
    const std::vector<ScoreRange>& score_ranges,
    AccumulatorArray& acc,
    const std::vector<pure_element_t<ScoreRange>>& term_weights,
    AccumulatorPolicy& accumulator_policy = default_accumulator)
{
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
//! TODO: Define the rest of the concepts
template<class PostingRange,
    class AccumulatorArray,
    class Weight = IdWeight,
    class AccumulatorPolicy = SimpleAccumulator,
    // TODO: Make sure that PostingList implements all required methods
    // CONCEPT_REQUIRES_(ranges::InputRange<PostingRange>()),
    CONCEPT_REQUIRES_(ranges::RandomAccessRange<AccumulatorArray>())>
void traverse_postings(const std::vector<PostingRange>& posting_ranges,
    AccumulatorArray& acc,
    const std::vector<score_t<pure_element_t<PostingRange>>>& term_weights,
    AccumulatorPolicy& accumulator_policy = default_accumulator)
{
    for (unsigned int term_id = 0; term_id < posting_ranges.size(); ++term_id) {
        traverse_list_postings(posting_ranges[term_id],
            acc,
            term_weights[term_id],
            accumulator_policy);
    }
}

template<class Result,
    class AccumulatorArray,
    CONCEPT_REQUIRES_(ranges::InputRange<AccumulatorArray>()),
    CONCEPT_REQUIRES_(ranges::Same<pure_element_t<AccumulatorArray>,
                      score_t<Result>>())>
std::vector<Result> aggregate_top(std::size_t k, const AccumulatorArray& acc)
{
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

};  // namespace irkit

