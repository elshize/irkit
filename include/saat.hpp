#pragma once

#include <assert.h>
#include <math.h>
#include "debug_assert.hpp"
#include "irkit/daat.hpp"
#include "irkit/taat.hpp"
#include "query.hpp"
#include "retrievers.hpp"

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

    virtual irk::Heap<Score, unsigned int>
    post_lists_by_score(const std::vector<PostingList>& term_postings,
        const std::vector<Score>& term_weights)
    {
        irk::Heap<Score, unsigned int> post_list_heap(term_postings.size());
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

    virtual nlohmann::json stats() { return {}; }
};

/// Implementation of Fagin's Threshold Algorithm.
///
/// Assumes that postings are sorted by their partial scores.
template<typename PostingList>
class ThresholdRetriever : public DaatRetriever<PostingList> {
private:
    nlohmann::json stats_;
    std::size_t collection_size_;

public:
    ThresholdRetriever(std::size_t collection_size)
        : collection_size_(collection_size)
    {}

    Score score_with_lookups(Doc doc, std::vector<Score>& acc)
    {
        return acc[doc];
    }

    virtual std::vector<Result> retrieve(
        const std::vector<PostingList>& term_postings,
        const std::vector<Score>& term_weights,
        std::size_t k)
    {

        // For efficiency reasons, accumulate all scores beforehand.
        std::vector<Score> acc(collection_size_, Score(0));
        std::vector<gsl::span<Doc>> doc_lists;
        std::vector<gsl::span<Score>> score_lists;
        for (const auto& postlist : term_postings) {
            doc_lists.push_back(postlist.docs);
            score_lists.push_back(postlist.scores);
        }
        irk::traverse(doc_lists, score_lists, acc, term_weights);

        auto iterators = DaatRetriever<PostingList>::to_iterators(term_postings);
        std::list<unsigned int> postlists;

        std::size_t all_postings = 0;
        for (unsigned int idx = 0; idx < term_postings.size(); ++idx) {
            if (!term_postings[idx].empty()) {
                postlists.push_back(idx);
                all_postings += term_postings[idx].length();
            }
        }
        stats_["postings"] = all_postings;

        std::size_t lookups = 0;
        std::size_t traversed = 0;

        irk::Heap<Score, Doc> top_results(k);
        std::set<Doc> visited_docs;
        while (!postlists.empty()) {
            traversed += postlists.size();
            Score threshold = Score(0);
            std::list<unsigned int> to_remove;
            for (unsigned int pidx : postlists) {
                auto& current = iterators[pidx].current;
                auto [doc, score] = *current;
                threshold += score * term_weights[pidx];
                if (visited_docs.find(doc) == visited_docs.end()) {
                    lookups += postlists.size() - 1;
                    Score doc_score = score_with_lookups(doc, acc);
                    top_results.push_with_limit(doc_score, doc, k);
                    visited_docs.insert(doc);
                }
                ++current;
                if (current == iterators[pidx].end) {
                    to_remove.push_back(pidx);
                }
            }
            if (top_results.size() == k && top_results.top().key >= threshold) {
                break;
            }
            for (auto pidx : to_remove) {
                postlists.remove(pidx);
            }
        }
        stats_["traversed"] = traversed;
        stats_["lookups"] = lookups;
        return heap_to_results(top_results);
    }

    virtual nlohmann::json stats() { return stats_; }
};

};  // namespace bloodhound::query
