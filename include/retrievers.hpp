#pragma once

#include <list>
#include <range/v3/numeric/accumulate.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>
#include <set>
#include "irkit/daat.hpp"
#include "irkit/taat.hpp"
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
    virtual irkit::Heap<Doc, unsigned int> post_lists_by_doc(
        const std::vector<PostingList>& term_postings)
    {
#ifdef STATS
        std::size_t postings = 0;
#endif
        irkit::Heap<Doc, unsigned int> post_list_heap(term_postings.size());
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
        irkit::Heap<Score, Doc> top_results_heap(k);
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
private:
    nlohmann::json stats_;

public:
    //    // Selects the pivot and returns all posting lists that are at or
    //    before
    //    // the pivot doc ID.
    //    std::vector<irkit::Entry<Doc, unsigned int>> select_pivot(
    //        const std::vector<PostingList>& term_postings,
    //        irkit::Heap<Doc, unsigned int>& post_list_heap,
    //        const std::vector<Score>& term_weights,
    //        const Score threshold)
    //    {
    //        DEBUG_ASSERT(!post_list_heap.empty(), debug{});
    //        DEBUG_ASSERT(!term_postings.empty(), debug{});
    //        Score max_sum = Score(0);
    //        std::vector<irkit::Entry<Doc, unsigned int>> buffer;
    //        while (!post_list_heap.empty()) {
    //            auto top = post_list_heap.pop();
    //            auto post_idx = top.value;
    //            buffer.push_back(top);
    //            auto max_score = term_postings[post_idx].max_score;
    //            max_sum += max_score * term_weights[post_idx];
    //            if (max_sum >= threshold)
    //                break;
    //        }
    //        Doc pivot_doc = buffer[buffer.size() - 1].key;
    //        while (
    //            !post_list_heap.empty() && post_list_heap.top().key ==
    //            pivot_doc) { buffer.push_back(post_list_heap.pop());
    //        }
    //#ifdef STATS
    //        std::cout << "PIVOT\t" << buffer.size() << std::endl;
    //#endif
    //        return buffer;
    //    }

    //    virtual std::vector<Result> retrieve(
    //        const std::vector<PostingList>& term_postings,
    //        const std::vector<Score>& term_weights,
    //        std::size_t k)
    //    {
    //        std::size_t all_postings = 0;
    //        for (auto& postlist : term_postings) {
    //            all_postings += postlist.length();
    //        }
    //        stats_["postings"] = all_postings;
    //        std::size_t next_ge_count = 0;
    //#ifdef STATS
    //        std::size_t evaluations = 0;
    //        std::size_t pivot_selections = 0;
    //#endif
    //        auto iterators =
    //            DaatRetriever<PostingList>::to_iterators(term_postings);
    //        auto post_list_heap =
    //            DaatRetriever<PostingList>::post_lists_by_doc(term_postings);
    //        Score threshold = Score(0);
    //
    //        irkit::Heap<Score, Doc> top_results_heap(k);
    //        while (!post_list_heap.empty()) {
    //            /* Select pivot */
    //            auto pivot_prefix = select_pivot(
    //                term_postings, post_list_heap, term_weights, threshold);
    //#ifdef STATS
    //            ++pivot_selections;
    //#endif
    //            Doc pivot_doc = pivot_prefix[pivot_prefix.size() - 1].key;
    //            if (pivot_prefix[0].key == pivot_doc) {
    //                // All at the same document ID: score and accumulate.
    //                Score score = Score(0);
    //                for (auto& entry : pivot_prefix) {
    //#ifdef STATS
    //                    ++evaluations;
    //#endif
    //                    int post_idx = entry.value;
    //                    auto curscore = iterators[post_idx].current->score;
    //                    Score weight = term_weights[post_idx];
    //                    score += curscore * weight;
    //                    ++iterators[post_idx].current;
    //                    if (iterators[post_idx].current
    //                        != iterators[post_idx].end) {
    //                        post_list_heap.push(
    //                            iterators[post_idx].current->doc, post_idx);
    //                    }
    //                }
    //                top_results_heap.push_with_limit(score, pivot_doc, k);
    //                if (top_results_heap.size() == k) {
    //                    threshold = top_results_heap.top().key;
    //                }
    //            } else {
    //                // Move all lists to the pivot ID and push back to the
    //                heap if
    //                // not finished.
    //                for (auto& entry : pivot_prefix) {
    //                    auto post_idx = entry.value;
    //                    auto& post_list = term_postings[post_idx];
    //                    auto& current = iterators[post_idx].current;
    //                    auto& current_doc = current->doc;
    //                    if (current_doc < pivot_doc) {
    //                        current = post_list.next_ge(current, pivot_doc);
    //                        ++next_ge_count;
    //                    }
    //                    if (current != iterators[post_idx].end) {
    //                        post_list_heap.push(current_doc, post_idx);
    //                    }
    //                }
    //            }
    //        }
    //        stats_["processed"] = next_ge_count;
    //#ifdef STATS
    //        std::cout << "NEXTGE\t" << next_ge_count << std::endl;
    //        std::cout << "EVALUATIONS\t" << pivot_selections << std::endl;
    //        std::cout << "PIVOT_SELECTIONS\t" << evaluations << std::endl;
    //#endif
    //        return heap_to_results(top_results_heap);
    //    }

    virtual std::vector<Result> retrieve(
        const std::vector<PostingList>& term_postings,
        const std::vector<Score>& term_weights,
        std::size_t k)
    {
        auto r = irkit::wand(term_postings, k, term_weights);
        std::vector<query::Result> results;
        for (auto & [doc, score] : r) {
            results.push_back({doc, score});
        }
        return results;
    }

    virtual nlohmann::json stats() { return stats_; }
};

// DAAT MaxScore query processor
template<typename PostingList>
class MaxScoreRetriever : public DaatRetriever<PostingList> {

    struct List {
        unsigned int term;
        Score max_score;
    };

    struct Partition {
        std::vector<List> essential;
        std::vector<List> non_essential;
    };

    Partition post_lists_by_max_score(std::vector<PostingList>& term_postings,
        const std::vector<Score>& term_weights)
    {
#ifdef STATS
        std::size_t postings = 0;
#endif
        std::vector<List> essential;
        std::vector<List> non_essential;
        for (unsigned int idx = 0; idx < term_postings.size(); ++idx) {
            if (!term_postings[idx].empty()) {
                auto max_score =
                    term_postings[idx].max_score * term_weights[idx];
                essential.push_back({idx, max_score});
#ifdef STATS
                postings += term_postings[idx].length();
#endif
            }
        }
#ifdef STATS
        std::cout << "POSTINGS\t" << postings << std::endl;
#endif

        // Sort by decreasing max score so that we can pop_back
        // to move the smallest essential to the non-essential list.
        std::sort(essential.begin(),
            essential.end(),
            [](const List& a, const List& b) {
                return a.max_score > b.max_score;
            });
        return {essential, non_essential};
    }

    /// Moves essential lists to non-essential container if threshold available.
    /// Returns true if partition changed, false otherwise.
    bool update_non_essential(Partition& partition, Score available_threshold)
    {
        if (partition.essential[partition.essential.size() - 1].max_score
            <= available_threshold) {
            while (partition.essential[partition.essential.size() - 1].max_score
                <= available_threshold) {
                List list = partition.essential[partition.essential.size() - 1];
                partition.non_essential.push_back(list);
                partition.essential.pop_back();
                available_threshold -= list.max_score;
            }
            return true;
        }
        return false;
    }

    virtual std::vector<Result> process(std::vector<PostingList>& term_postings,
        const std::vector<Score>& term_weights,
        std::size_t k)
    {
        Partition partition =
            post_lists_by_max_score(term_postings, term_weights);
        auto iterators =
            DaatRetriever<PostingList>::to_iterators(term_postings);
        auto essential_by_doc =
            DaatRetriever<PostingList>::post_lists_by_doc(term_postings);

        Score threshold = Score(0);
        Score non_essential_max_sum = Score(0);

        irkit::Heap<Score, Doc> top_results_heap(k);

        while (!partition.essential.empty()) {
            bool rebuild_heap = update_non_essential(
                partition, threshold - non_essential_max_sum);
            if (rebuild_heap) {
                essential_by_doc.clear();
                for (auto& list : partition.essential) {
                    if (iterators[list.term].current
                        != iterators[list.term].end) {
                        essential_by_doc.push(
                            iterators[list.term].current->doc, list.term);
                    }
                }
            }
            Doc min_doc = essential_by_doc.top().key;
            Score score = Score(0);
            while (!essential_by_doc.empty()
                && essential_by_doc.top().key == min_doc) {
                auto top = essential_by_doc.top();
                auto post_idx = top.value;
                auto curscore = iterators[post_idx].current->score;
                score += curscore * term_weights[post_idx];
                ++iterators[post_idx].current;
                if (iterators[post_idx].current != iterators[post_idx].end) {
                    essential_by_doc.pop_push(
                        iterators[post_idx].current->doc, post_idx);
                } else {
                    essential_by_doc.pop();
                }
            }
            if (score + non_essential_max_sum >= threshold) {
                for (auto& list : partition.non_essential) {
                    auto& current = iterators[list.term].current;
                    current =
                        term_postings[list.term].next_ge(current, min_doc);
                    auto & [current_doc, current_score] = *current;
                    if (min_doc == current_doc) {
                        score += current_score;
                    }
                }
                top_results_heap.push_with_limit(score, min_doc, k);
                if (top_results_heap.size() == k) {
                    threshold = top_results_heap.top().key;
                }
            }
        }
        return heap_to_results(top_results_heap);
    }
};

using QueryId = unsigned char;


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
          bits_to_shift(sizeof(Score) * 8 - irkit::nbits(init_gap)),
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
    void accumulate_posting(Doc doc, Score score_delta, std::vector<Score>& acc)
    {
        if constexpr (init_gap > 1) {
            auto old_score = acc[doc];
            acc[doc] = old_score < qidx_shifted
                ? score_delta | qidx_shifted
                : old_score + score_delta;
        } else {
            acc[doc] += score_delta;
        }
        if constexpr (acc_block_size > 1) {
            constexpr std::size_t block_nbits = irkit::nbits(acc_block_size);
            std::size_t block = doc >> block_nbits;
            block_max_scores[block] =
                std::max(acc[doc], block_max_scores[block]);
        }
    }

    /// Traverses a single posting lists and accumulates its scores.
    //void traverse_list(const PostingList& postlist, Score term_weight)
    //{
    //    auto dociter = postlist.doc_begin();
    //    auto scoreiter = postlist.score_begin();

    //    std::size_t prefetch_ahead = 3;
    //    auto docend = postlist.length() >= 3
    //        ? postlist.doc_end() - prefetch_ahead
    //        : postlist.doc_begin();
    //    for (; dociter != docend; ++dociter, ++scoreiter) {
    //        //if constexpr (prefetch) {
    //        //    auto to_fetch = *(dociter + 3);
    //        //    //__builtin_prefetch(dociter + 16, 0, 0);
    //        //    //__builtin_prefetch(scoreiter + 16, 0, 0);
    //        //    //__builtin_prefetch(
    //        //    //    accumulator_array.data() + to_fetch, 1, 0);
    //        //}
    //        auto score_delta = *scoreiter * term_weight;
    //        accumulate_posting(*dociter, score_delta);
    //    }
    //    docend = postlist.doc_end();
    //    for (; dociter != docend; ++dociter, ++scoreiter) {
    //        auto score_delta = *scoreiter * term_weight;
    //        accumulate_posting(*dociter, score_delta);
    //    }
    //}

    /// Traverses the postings and accumulates the scores.
    void traverse(const std::vector<PostingList>& lists_for_terms,
        const std::vector<Score>& term_weights)
    {
        std::vector<gsl::span<Doc>> doc_lists;
        std::vector<gsl::span<Score>> score_lists;
        for (const auto& postlist : lists_for_terms) {
            doc_lists.push_back(postlist.docs);
            score_lists.push_back(postlist.scores);
        }
        irkit::traverse(doc_lists,
            score_lists,
            accumulator_array,
            term_weights,
            *this);
        //for (std::size_t term = 0; term < lists_for_terms.size(); ++term) {
        //    auto& postlist = lists_for_terms[term];
        //    //traverse_list(postlist, term_weights[term]);
        //    taat<prefetch, init_gap, acc_block_size>::traverse_list(
        //        postlist.docs,
        //        postlist.scores,
        //        //Range(postlist.score_begin(), postlist.score_end()),
        //        accumulator_array,
        //        term_weights[term]);
        //}
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
        irkit::Heap<Score, Doc> heap(k);
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
class MaxScoreNonEssentials {
private:
    std::size_t collection_size_;
    nlohmann::json stats_;

public:
    MaxScoreNonEssentials(std::size_t collection_size)
        : collection_size_(collection_size)
    {}

    std::pair<Score, std::vector<Result>> calc_threshold(
        const std::vector<PostingList>& lists_for_terms,
        const std::vector<Score>& term_weights,
        std::size_t k)
    {
        std::vector<Score> acc(collection_size_, Score(0));
        std::vector<gsl::span<Doc>> doc_lists;
        std::vector<gsl::span<Score>> score_lists;
        for (const auto& postlist : lists_for_terms) {
            doc_lists.push_back(postlist.docs);
            score_lists.push_back(postlist.scores);
        }
        irkit::traverse(doc_lists, score_lists, acc, term_weights);
        auto top_k = irkit::aggregate_top<bloodhound::query::Result>(k, acc);
        Score threshold = top_k.size() == k ? top_k[k - 1].score : Score(0);
        return {threshold, top_k};
    }

    static bool compare_len(const PostingList& a, const PostingList& b)
    {
        return a.length() < b.length();
    }

    static bool compare_maxscore(const PostingList& a, const PostingList& b)
    {
        return a.max_score > b.max_score;
    }

    std::size_t run_for(std::string type,
        bool (*compare)(const PostingList&, const PostingList&),
        std::vector<PostingList>& lists_for_terms,
        const std::vector<Score>& term_weights,
        std::size_t k,
        Score threshold)
    {
        std::sort(lists_for_terms.begin(), lists_for_terms.end(), compare);
        // Non-essential are at the end of the list.
        Score maxscore_sum = ranges::accumulate(
            ranges::view::zip(lists_for_terms, term_weights)
                | ranges::view::transform([](auto list_weight) {
                      return list_weight.first.max_score * list_weight.second;
                  }),
            Score(0));
        stats_["max_score_sum"] = type_safe::get(maxscore_sum);

        std::size_t num_ess = 0;
        std::size_t postings = 0;
        Score remaining_maxscore_sum = maxscore_sum;
        std::set<Doc> visited_docs;
        std::vector<Score> acc(collection_size_, Score(0));

        // Iterate over **essential** lists
        while (remaining_maxscore_sum > threshold) {
            auto& postlist = lists_for_terms[num_ess];
            auto dociter = postlist.doc_begin();
            auto scoreiter = postlist.score_begin();
            for (; dociter != postlist.doc_end(); ++dociter, ++scoreiter) {
                acc[*dociter] += *scoreiter;
                visited_docs.insert(*dociter);
            }
            remaining_maxscore_sum -=
                postlist.max_score * term_weights[num_ess];
            postings += postlist.length();
            ++num_ess;
        }
        stats_["essential_terms_" + type] = num_ess;
        stats_["nonessential_terms_" + type] = lists_for_terms.size() - num_ess;
        stats_["essential_postings_" + type] = postings;
        stats_["essential_docs_" + type] = visited_docs.size();

        std::size_t nonessential_updates = 0;
        for (auto idx = num_ess; idx < lists_for_terms.size(); ++idx) {
            auto& postlist = lists_for_terms[idx];
            auto dociter = postlist.doc_begin();
            auto scoreiter = postlist.score_begin();
            for (; dociter != postlist.doc_end(); ++dociter, ++scoreiter) {
                if (acc[*dociter] > Score(0)) {
                    ++nonessential_updates;
                }
            }
            postings += lists_for_terms[idx].length();
        }
        stats_["nonessential_updates_" + type] = nonessential_updates;
        stats_["allpost_" + type] =
            stats_["essential_postings_" + type].get<std::size_t>()
            + nonessential_updates;
        return postings;
    }

    virtual std::vector<Result> retrieve(
        const std::vector<PostingList>& lists_for_terms,
        const std::vector<Score>& term_weights,
        std::size_t k)
    {
        std::vector<std::size_t> lengths;
        std::vector<uint32_t> max_scores;
        for (auto [postlist, weight] :
            ranges::view::zip(lists_for_terms, term_weights)) {
            lengths.push_back(postlist.length());
            max_scores.push_back(type_safe::get(postlist.max_score * weight));
        }
        stats_["lengths"] = lengths;
        stats_["max_scores"] = max_scores;

        auto [threshold, results] = calc_threshold(lists_for_terms, term_weights, k);
        stats_["threshold"] = type_safe::get(threshold);
        stats_["terms"] = lists_for_terms.size();

        std::vector<PostingList> postlists = lists_for_terms;
        auto postings = run_for("len", compare_len, postlists, term_weights, k, threshold);
        run_for("ms", compare_maxscore, postlists, term_weights, k, threshold);
        stats_["postings"] = postings;

        return results;
    }

    virtual nlohmann::json stats()
    {
        return stats_;
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
        irkit::Heap<Score, Doc> heap(k);
        auto size = static_cast<std::size_t>(accumulator_array.size());
        for (Doc doc = Doc(0); doc < size; ++doc) {
            Score score = accumulator_array[doc];
            heap.push_with_limit(score, doc, k);
        }
        return heap_to_results(heap);
    }
};


template<typename PostingList>
class TaatMaxScoreRetriever : public Retriever<PostingList> {
public:
    TaatMaxScoreRetriever(std::size_t collection_size)
        : collection_size(collection_size)
    {}

    std::vector<std::size_t> sorted_by_length(
        const std::vector<PostingList>& lists_for_terms)
    {
        std::vector<std::size_t> sorted;
        for (std::size_t idx = 0; idx < lists_for_terms.size(); ++idx) {
            if (lists_for_terms[idx].length() > 0) {
                sorted.push_back(idx);
            }
        }
        std::sort(sorted.begin(),
            sorted.end(),
            [lists_for_terms](const std::size_t& a, const std::size_t& b) {
                return lists_for_terms[a].length()
                    < lists_for_terms[b].length();
            });
        return sorted;
    }

    virtual std::vector<Result> retrieve(
        const std::vector<PostingList>& lists_for_terms,
        const std::vector<Score>& term_weights,
        std::size_t k)
    {
        std::vector<Score> acc(collection_size, Score(0));
        irkit::Heap<Score, Doc, std::less<Score>, std::unordered_map<Doc, int>>
            top_results(k);

        auto postlists = sorted_by_length(lists_for_terms);

        Score threshold = Score(0);
        Score remaining_max = Score(0);
        for (auto& term : postlists) {
            remaining_max += lists_for_terms[term].max_score;
        }

        std::size_t postings = 0;
        std::size_t idx = 0;
        while (idx < postlists.size() && threshold <= remaining_max) {
            auto& postlist = lists_for_terms[idx];
            auto dociter = postlist.doc_begin();
            auto scoreiter = postlist.score_begin();
            for (; dociter != postlist.doc_end(); ++dociter, ++scoreiter) {
                auto doc = *dociter;
                auto score_delta = *scoreiter * term_weights[idx];
                acc[doc] += score_delta;
                top_results.push_with_limit(acc[doc], doc, k);
            }
            remaining_max -= postlist.max_score;
            if (top_results.size() == k) {
                threshold = top_results.top().key;
            }
            postings += postlist.length();
            idx++;
        }

        std::list<Doc> visited;
        for (Doc doc = Doc(0); doc < Doc(acc.size()); ++doc) {
            if (acc[doc] > Score(0)) {
                visited.push_back(doc);
            }
        }

        std::size_t first_phase = postings;
        std::size_t second_phase = 0;
        std::size_t next_ge_count = 0;
        std::size_t removed = 0;
        for (; idx < postlists.size(); ++idx) {
            auto& postlist = lists_for_terms[idx];
            auto iter = postlist.begin();
            auto end = postlist.end();
            auto visited_iter = visited.begin();
            auto visited_end = visited.end();
            while (visited_iter != visited_end) {
                Doc doc = *visited_iter;
                ++next_ge_count;
                iter = postlist.next_ge(iter, doc);
                if (iter == end) {
                    break;
                }
                if (iter->doc == doc) {
                    acc[doc] += iter->score;
                    if (acc[doc] + remaining_max < threshold) {
                        visited_iter = visited.erase(visited_iter);
                        ++removed;
                        continue;
                    } else {
                        top_results.push_with_limit(acc[doc], doc, k);
                        if (top_results.size() == k) {
                            threshold = top_results.top().key;
                        }
                    }
                }
                ++visited_iter;
            }
            remaining_max -= postlist.max_score;
            second_phase += postlist.length();
        }
        std::cout << first_phase
            << "," << second_phase
            << "," << next_ge_count
            << "," << first_phase + second_phase
            << "," << removed
            << std::endl;
        return heap_to_results(top_results);
    }

private:
    std::size_t collection_size;
};

};  // namespace bloodhound::query
