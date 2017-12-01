#pragma once

#include "query.hpp"

namespace bloodhound::query {

/// Document-at-a-time query processor
template<typename PostingList>
class DaatRetriever : public Retriever<PostingList> {

    static_assert(has_posting_iterator<PostingList>::value,
        "the PostingList type must implement a posting iterator");

protected:
    /// Current and end iterators of the same posting list.
    struct IteratorPair {
        typename PostingList::iterator current;
        typename PostingList::iterator end;
    };

public:
    /// Returns initial min-heap of posting lists sorted by their current Doc.
    virtual Heap<Doc, unsigned int> post_lists_by_doc(
        const std::vector<PostingList>& term_postings)
    {
#ifdef STATS
        std::size_t postings = 0;
#endif
        Heap<Doc, unsigned int> post_list_heap(term_postings.size());
        for (unsigned int idx = 0; idx < term_postings.size(); ++idx) {
            if (!term_postings[idx].empty()) {
                Doc doc = term_postings[idx].begin()->doc;
                post_list_heap.push(doc, idx);
#ifdef STATS
                postings += term_postings[idx].length();
#endif
            }
        }
#ifdef STATS
        std::cout << "POSTINGS\t" << postings << std::endl;
#endif
        return post_list_heap;
    }

    virtual std::vector<IteratorPair> to_iterators(
        const std::vector<PostingList>& term_postings)
    {
        std::vector<IteratorPair> iterators;
        for (const auto& posting_list : term_postings) {
            iterators.push_back({posting_list.begin(), posting_list.end()});
        }
        return iterators;
    }

    virtual std::vector<Result> retrieve(
        const std::vector<PostingList>& term_postings,
        const std::vector<Score>& term_weights,
        std::size_t k)
    {
        auto iterators = to_iterators(term_postings);
        auto post_list_heap = post_lists_by_doc(term_postings);
        Heap<Score, Doc> top_results_heap(k);
        while (!post_list_heap.empty()) {
            Doc min_doc = post_list_heap.top().key;
            Score score = Score(0);
            while (!post_list_heap.empty()
                && post_list_heap.top().key == min_doc) {
                auto top = post_list_heap.top();
                auto post_idx = top.value;
                auto curscore = iterators[post_idx].current->score;
                score += curscore * term_weights[post_idx];
                ++iterators[post_idx].current;
                if (iterators[post_idx].current != iterators[post_idx].end) {
                    post_list_heap.pop_push(
                        iterators[post_idx].current->doc, post_idx);
                } else {
                    post_list_heap.pop();
                }
            }
            top_results_heap.push_with_limit(score, min_doc, k);
        }
        return heap_to_results(top_results_heap);
    }
};

/// WAND (Weak-AND) query retriever
template<typename PostingList>
class WandRetriever : public DaatRetriever<PostingList> {

public:
    // Selects the pivot and returns all posting lists that are at or before
    // the pivot doc ID.
    std::vector<Heap<Doc, unsigned int>::Entry> select_pivot(
        const std::vector<PostingList>& term_postings,
        Heap<Doc, unsigned int>& post_list_heap,
        const std::vector<Score>& term_weights,
        const Score threshold)
    {
        DEBUG_ASSERT(!post_list_heap.empty(), debug{});
        DEBUG_ASSERT(!term_postings.empty(), debug{});
        Score max_sum = Score(0);
        std::vector<Heap<Doc, unsigned int>::Entry> buffer;
        while (!post_list_heap.empty()) {
            auto top = post_list_heap.pop();
            auto post_idx = top.value;
            buffer.push_back(top);
            auto max_score = term_postings[post_idx].max_score.value();
            max_sum += max_score * term_weights[post_idx];
            if (max_sum >= threshold)
                break;
        }
        Doc pivot_doc = buffer[buffer.size() - 1].key;
        while (
            !post_list_heap.empty() && post_list_heap.top().key == pivot_doc) {
            buffer.push_back(post_list_heap.pop());
        }
#ifdef STATS
        std::cout << "PIVOT\t" << buffer.size() << std::endl;
#endif
        return buffer;
    }

    virtual std::vector<Result> retrieve(
        const std::vector<PostingList>& term_postings,
        const std::vector<Score>& term_weights,
        std::size_t k)
    {
#ifdef STATS
        std::size_t next_ge_count = 0;
        std::size_t evaluations = 0;
        std::size_t pivot_selections = 0;
#endif
        auto iterators =
            DaatRetriever<PostingList>::to_iterators(term_postings);
        auto post_list_heap =
            DaatRetriever<PostingList>::post_lists_by_doc(term_postings);
        Score threshold = Score(0);

        Heap<Score, Doc> top_results_heap(k);
        while (!post_list_heap.empty()) {
            /* Select pivot */
            auto pivot_prefix = select_pivot(
                term_postings, post_list_heap, term_weights, threshold);
#ifdef STATS
            ++pivot_selections;
#endif
            Doc pivot_doc = pivot_prefix[pivot_prefix.size() - 1].key;
            if (pivot_prefix[0].key == pivot_doc) {
                // All at the same document ID: score and accumulate.
                Score score = Score(0);
                for (auto& entry : pivot_prefix) {
#ifdef STATS
                    ++evaluations;
#endif
                    int post_idx = entry.value;
                    auto curscore = iterators[post_idx].current->score;
                    Score weight = term_weights[post_idx];
                    score += curscore * weight;
                    ++iterators[post_idx].current;
                    if (iterators[post_idx].current
                        != iterators[post_idx].end) {
                        post_list_heap.push(
                            iterators[post_idx].current->doc, post_idx);
                    }
                }
                top_results_heap.push_with_limit(score, pivot_doc, k);
                if (top_results_heap.size() == k) {
                    threshold = top_results_heap.top().key;
                }
            } else {
                // Move all lists to the pivot ID and push back to the heap if
                // not finished.
                for (auto& entry : pivot_prefix) {
                    auto post_idx = entry.value;
                    auto& post_list = term_postings[post_idx];
                    auto& current = iterators[post_idx].current;
                    auto& current_doc = current->doc;
                    if (current_doc < pivot_doc) {
                        current = post_list.next_ge(current, pivot_doc);
#ifdef STATS
                        ++next_ge_count;
#endif
                    }
                    if (current != iterators[post_idx].end) {
                        post_list_heap.push(current_doc, post_idx);
                    }
                }
            }
        }
#ifdef STATS
        std::cout << "NEXTGE\t" << next_ge_count << std::endl;
        std::cout << "EVALUATIONS\t" << pivot_selections << std::endl;
        std::cout << "PIVOT_SELECTIONS\t" << evaluations << std::endl;
#endif
        return heap_to_results(top_results_heap);
    }
};

// DAAT MaxScore query processor
// class MaxScoreProcessor : public DaatProcessor {
//
//    struct List {
//        unsigned int term;
//        Score max_score;
//    };
//
//    struct Partition {
//        std::vector<List> essential;
//        std::vector<List> non_essential;
//    };
//
//    Partition
//    post_lists_by_max_score(std::vector<PostingList>& term_postings)
//    {
//#ifdef STATS
//        std::size_t postings = 0;
//#endif
//        std::vector<List> essential;
//        std::vector<List> non_essential;
//        for (unsigned int idx = 0; idx < term_postings.size(); ++idx) {
//            if (!term_postings[idx].empty()) {
//                auto max_score = term_postings[idx].max_score.value();
//                essential.push_back({ idx, max_score });
//#ifdef STATS
//                postings += term_postings[idx].length();
//#endif
//            }
//        }
//#ifdef STATS
//        std::cout << "POSTINGS\t" << postings << std::endl;
//#endif
//
//        // Sort by decreasing max score so that we can pop_back
//        // to move the smallest essential to the non-essential list.
//        std::sort(
//            essential.begin(),
//            essential.end(),
//            [](const List& a, const List& b) {
//                return a.max_score > b.max_score;
//            });
//        return { essential, non_essential };
//    }
//
//    void update_non_essential(Partition& partition, Score threshold)
//    {
//    }
//
//    virtual std::vector<Result>
//    process(
//        std::vector<PostingList>& term_postings,
//        const std::vector<Score>& term_weights,
//        std::size_t k)
//    {
//        Partition partition = post_lists_by_max_score(term_postings);
//        Score threshold = Score(0);
//
//        while (!partition.essential.empty()) {
//            update_non_essential(partition, threshold);
//        }
//    }
//};

} // namespace bloodhound::query
