#pragma once

#include <type_traits>
#include "query.hpp"

namespace bloodhound::query {

using QueryId = unsigned char;

/// Computes the number of bits required to store an integer n.
template<typename Integer>
constexpr unsigned short nbits(Integer n)
{
    unsigned short bits = 0;
    while (n >>= 1)
        ++bits;
    return bits;
}

/// Term-at-a-time document retriever.
///
/// PostingList requires:
/// * doc_begin() and doc_end(): document start and end iterator,
///   respectively;
/// * score_begin() and score_end(): score start and end iterator,
///   respectively;
/// * length(): posting list length;
template<typename PostingList,
    bool prefetch = false,
    unsigned short init_gap = 0,
    unsigned int acc_block_size = 0>
class TaatRetriever : public Retriever<PostingList> {

protected:
    /// Keeps track of the accumulator clearing cycle.
    /// It is always in range [0, init_gap), and is increased modulo init_gap
    /// after each query. This is only used when init_gap > 1.
    QueryId query_id;

    /// This is query_id shifted to the far left for quick comparison of the
    /// accumulated values. This is only used when init_gap > 1.
    Score qidx_shifted;

    /// The score mask for fast computing of the accumulated values. This is
    /// only used when init_gap > 1.
    Score score_mask;

    /// How many bits are used for the score value. This is only used when
    /// init_gap > 1.
    unsigned int bits_to_shift;

    /// The array of accumulated values for each document.
    std::vector<Score> accumulator_array;

    /// Maximum scores for each accumulator block.
    /// Only used when acc_block_size > 0.
    std::vector<Score> block_max_scores;

    /// The number of blocks, calculated based on acc_block_size and the number
    /// of documents in the index. Only used when acc_block_size > 0.
    // template<class = std::enable_if_t<acc_block_size > 0, void>>
    std::size_t nblocks;

public:
    /// Constructs a TaatRetriever with an accumulator array of collection_size.
    TaatRetriever(std::size_t collection_size)
        : query_id(0),
          qidx_shifted(0),
          score_mask(0),
          bits_to_shift(sizeof(Score) * 8 - nbits(init_gap)),
          accumulator_array(collection_size, Score(0))
    {
        static_assert(
            (init_gap & (init_gap - 1)) == 0, "init_gap must be a power of 2");
        static_assert((acc_block_size & (acc_block_size - 1)) == 0,
            "acc_block_size must be a power of 2");
        if constexpr (acc_block_size > 0) {
            nblocks = (collection_size + acc_block_size - 1) / acc_block_size;
            block_max_scores.resize(nblocks, Score(0));
        }
        if constexpr (init_gap > 1) {
            score_mask = Score((1 << bits_to_shift) - 1);
        }
    }

    /// Accumulates the posting that is being processed.
    void accumulate_posting(Doc doc, Score score_delta)
    {
        if constexpr (init_gap > 1) {
            auto old_score = accumulator_array[doc];
            accumulator_array[doc] = old_score < qidx_shifted
                ? score_delta | qidx_shifted
                : old_score + score_delta;
        } else {
            accumulator_array[doc] += score_delta;
        }
        if constexpr (acc_block_size > 1) {
            constexpr std::size_t block_nbits = nbits(acc_block_size);
            std::size_t block = doc >> block_nbits;
            block_max_scores[block] =
                std::max(accumulator_array[doc], block_max_scores[block]);
        }
    }

    /// Traverses the postings and accumulates the scores.
    void traverse(const std::vector<PostingList>& lists_for_terms,
        const std::vector<Score>& term_weights)
    {
        for (std::size_t term = 0; term < lists_for_terms.size(); ++term) {
            auto& posting_list = lists_for_terms[term];
            auto dociter = posting_list.doc_begin();
            auto scoreiter = posting_list.score_begin();
            Score term_weight = term_weights[term];

            std::size_t prefetch_ahead = 3;
            auto docend = posting_list.length() >= 3
                ? posting_list.doc_end() - prefetch_ahead
                : posting_list.doc_begin();
            for (; dociter != docend; ++dociter, ++scoreiter) {
                if constexpr (prefetch) {
                    auto to_fetch = *(dociter + 3);
                    __builtin_prefetch(dociter + 16, 0, 0);
                    __builtin_prefetch(scoreiter + 16, 0, 0);
                    __builtin_prefetch(
                        accumulator_array.data() + to_fetch, 1, 0);
                }
                auto score_delta = *scoreiter * term_weight;
                accumulate_posting(*dociter, score_delta);
            }
            docend = posting_list.doc_end();
            for (; dociter != docend; ++dociter, ++scoreiter) {
                auto score_delta = *scoreiter * term_weight;
                accumulate_posting(*dociter, score_delta);
            }
        }
    }

    /// Returns the accumulated score of doc.
    Score score_of(Doc doc) const
    {
        Score score = accumulator_array[doc];
        if constexpr (init_gap > 1) {
            return score < qidx_shifted ? Score(0) : score & score_mask;
        } else {
            return score;
        }
    }

    /// Returns the top-k highest ranked documents.
    ///
    /// It may return fewer documents if fewer of them contain any term.
    std::vector<Result> aggregate_top(std::size_t k)
    {
        Heap<Score, Doc> heap(k);
        if constexpr (acc_block_size > 1) {
            for (std::size_t block = 0; block < nblocks; ++block) {
                auto size = static_cast<std::size_t>(std::min(
                    accumulator_array.size(), ((block + 1) * acc_block_size)));
                auto threshold = heap.size() == k ? heap.top().key : Score(0);
                if (block_max_scores[block] < threshold) {
                    continue;
                }
                for (Doc doc = Doc(block * acc_block_size); doc < size; ++doc) {
                    Score score = score_of(doc);
                    heap.push_with_limit(score, doc, k);
                }
            }
        } else {
            auto size = static_cast<std::size_t>(accumulator_array.size());
            for (Doc doc = Doc(0); doc < size; ++doc) {
                Score score = score_of(doc);
                heap.push_with_limit(score, doc, k);
            }
        }
        return heap_to_results(heap);
    }

    /// Fill the accumulator array with zeroes.
    void clear_accumulator_array()
    {
        std::fill(accumulator_array.begin(), accumulator_array.end(), Score(0));
    }

    /// Set all block maximum scores with zeroes.
    void clear_blocks()
    {
        std::fill(block_max_scores.begin(), block_max_scores.end(), Score(0));
    }

    /// Proceed to the next query.
    ///
    /// Clears the accumulators if necessary and keeps track of the query IDs.
    void next_query()
    {
        if constexpr (init_gap > 1) {
            query_id = (query_id + 1) % init_gap;
            qidx_shifted = Score(query_id << bits_to_shift);
            if (query_id == 0)
                clear_accumulator_array();
        } else
            clear_accumulator_array();
        clear_blocks();
    }

    virtual std::vector<Result> retrieve(
        const std::vector<PostingList>& lists_for_terms,
        const std::vector<Score>& term_weights,
        std::size_t k)
    {
        traverse(lists_for_terms, term_weights);
        auto top_results = aggregate_top(k);
        next_query();
        return top_results;
    }
};

template<typename PostingList>
class RawTaatRetriever : public Retriever<PostingList> {

private:
    /// The array of accumulated values for each document.
    std::vector<Score> accumulator_array;

public:
    RawTaatRetriever(std::size_t collection_size)
        : accumulator_array(collection_size, Score(0))
    {}

    /// Traverses the postings and accumulates the scores.
    void traverse(const std::vector<PostingList>& lists_for_terms,
        const std::vector<Score>& term_weights)
    {
        for (std::size_t term = 0; term < lists_for_terms.size(); ++term) {
            const PostingList& posting_list = lists_for_terms[term];
            Doc* docs = posting_list.docs_ptr();
            Score* scores = posting_list.scores_ptr();
            Score term_weight = term_weights[term];

            std::size_t len = posting_list.length();
            for (std::size_t idx = 0; idx < len; ++idx) {
                Score score_delta = scores[idx] * term_weight;
                accumulator_array[docs[idx]] += score_delta;
            }
        }
    }

    virtual std::vector<Result> retrieve(
        const std::vector<PostingList>& lists_for_terms,
        const std::vector<Score>& term_weights,
        std::size_t k)
    {
        std::fill(accumulator_array.begin(), accumulator_array.end(), Score(0));
        traverse(lists_for_terms, term_weights);
        std::vector<Result> results;
        Heap<Score, Doc> heap(k);
        auto size = static_cast<std::size_t>(accumulator_array.size());
        for (Doc doc = Doc(0); doc < size; ++doc) {
            Score score = accumulator_array[doc];
            heap.push_with_limit(score, doc, k);
        }
        return heap_to_results(heap);
    }
};

};  // namespace bloodhound::query
