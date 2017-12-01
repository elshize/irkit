#pragma once

#include <assert.h>
#include <math.h>
#include "daat.hpp"
#include "debug_assert.hpp"
#include "query.hpp"
#include "taat.hpp"

namespace bloodhound::query {

// Score-at-a-time query processor.
//
// Requires postings to be sorted by the partial impacts.
template<typename PostingList>
class ExactSaatRetriever : public DaatRetriever<PostingList>,
                           public TaatRetriever<PostingList, false, 0, 0> {
private:
    double et_threshold;
    std::size_t posting_threshold;
    std::size_t postings_processed;
    std::size_t posting_count;

public:
    ExactSaatRetriever(std::size_t collection_size, double et_threshold = 1.0)
        : TaatRetriever<PostingList>(collection_size),
          et_threshold(et_threshold),
          posting_threshold(0),
          postings_processed(0),
          posting_count(0)
    {
        assert(et_threshold > 0 && et_threshold <= 1.0);
    }

    std::size_t count_postings(const std::vector<PostingList>& term_postings)
    {
        std::size_t posting_count = 0;
        for (auto& posting_list : term_postings) {
            posting_count += posting_list.length();
        }
        return posting_count;
    }

    virtual Heap<Score, unsigned int> post_lists_by_score(
        const std::vector<PostingList>& term_postings,
        const std::vector<Score>& term_weights)
    {
        Heap<Score, unsigned int> post_list_heap(term_postings.size());
        for (unsigned int idx = 0; idx < term_postings.size(); ++idx) {
            if (!term_postings[idx].empty()) {
                Score score = term_postings[idx].begin()->score;
                post_list_heap.push(score * term_weights[idx], idx);
            }
        }
        return post_list_heap;
    }

    virtual std::vector<Result> retrieve(
        const std::vector<PostingList>& term_postings,
        const std::vector<Score>& term_weights,
        std::size_t k)
    {
        auto iterators =
            DaatRetriever<PostingList>::to_iterators(term_postings);
        auto post_list_heap = post_lists_by_score(term_postings, term_weights);

        posting_count = count_postings(term_postings);
        posting_threshold = ceil(posting_count * et_threshold);
        postings_processed = 0;

        while (postings_processed < posting_threshold) {
            auto[score, postlist_idx] = post_list_heap.top();
            auto& current = iterators[postlist_idx].current;
            auto doc = current->doc;
            TaatRetriever<PostingList, false, 0, 0>::accumulator_array[doc] +=
                score;
            ++current;
            if (current != iterators[postlist_idx].end) {
                post_list_heap.pop_push(
                    current->score * term_weights[postlist_idx], postlist_idx);
            } else {
                post_list_heap.pop();
            }
            ++postings_processed;
        }

        auto results =
            TaatRetriever<PostingList, false, 0, 0>::aggregate_top(k);
        TaatRetriever<PostingList, false, 0, 0>::next_query();
        return results;
    }

    std::size_t get_processed_postings() { return postings_processed; }
    std::size_t get_posting_threshold() { return posting_threshold; }
    std::size_t get_posting_count() { return posting_count; }
    void set_et_threshold(double et)
    {
        if (et > 0 && et <= 1.0) {
            et_threshold = et;
        } else {
            throw std::invalid_argument(
                "et must be in (0,1] but is: " + std::to_string(et));
        }
    }
};

template<typename PostingList>
class ApproxSaatRetriever : public TaatRetriever<PostingList, false, 0, 0> {
};

};  // namespace bloodhound::query
