#pragma once

#include <algorithm>
#include "irkit/types.hpp"
#include "irkit/utils.hpp"
#include "query.hpp"

namespace irkit {

//! A container for two ends of iterators.
//! TODO: Needs to be generalized, for containers as well?
//! TODO: Implement iterator methods.
template<class Iter>
struct moving_range {
    Iter left;
    Iter right;
    moving_range() = default;
    moving_range(Iter first, Iter last) : left(first), right(last) {}
    bool empty() const { return left == right; }
    void advance() { ++left; };
    void advance(unsigned int n) { left += n; };
    Iter begin() const { return left; }
    Iter end() const { return right; }
    auto front() const { return *left; }
};

//! Represents the union of sorted ranges (posting lists).
//! TODO: Generalize.
//! TODO: Improve efficiency below 17 ms on BoVW.
template<class Range>
class UnionRange {
protected:
    using Posting = pure_element_t<Range>;
    using Score = score_t<Posting>;
    using Doc = doc_t<Posting>;

    /// Pair of document ID and term ID to use in DAAT term heap.
    struct docterm {
        Doc doc;
        unsigned int term;
        docterm(Doc d, unsigned int t) : doc(d), term(t) {}
        bool operator==(const docterm& rhs) const
        {
            return doc == rhs.doc && term == rhs.term;
        }
        bool operator<(const docterm& rhs) const { return doc > rhs.doc; }
    };

    const std::vector<Score>& weights_;
    const std::vector<Score>& max_scores_;
    std::vector<moving_range<const_iterator_t<Range>>> ranges_;
    std::vector<docterm> heap_;

    /// Initilizes moving ranges and heap.
    void init(const std::vector<Range>& query_postings)
    {
        for (auto& postinglist : query_postings) {
            ranges_.emplace_back(postinglist.cbegin(), postinglist.cend());
        }
        for (unsigned int term = 0; term < ranges_.size(); ++term) {
            if (!ranges_[term].empty()) {
                heap_.push_back({ranges_[term].front().doc, term});
            }
        }
        std::make_heap(heap_.begin(), heap_.end());
    }

    void nextge(unsigned int term, Doc doc)
    {
        while (ranges_[term].front().doc < doc) {
            ranges_[term].advance();
        }
    }

public:
    //! Creates new union range from posting lists and term weights.
    /*!
     * @pre Each posting list must be sorted by increasing document IDs.
     * @param query_postings   a vector of posting lists for all query terms
     * @param weights          a vector of term weights
     */
    UnionRange(const std::vector<Range>& query_postings,
        const std::vector<Score>& weights,
        const std::vector<Score>& max_scores = std::move(std::vector<Score>()))
        : weights_(weights), max_scores_(max_scores)
    {
        init(query_postings);
    }

    //! Returns the next posting's term without advancing to the next one.
    unsigned int peek_term() { return heap_[0].term; }

    //! Returns the next posting without advancing to the next one.
    Posting peek_posting() { return ranges_[heap_[0].term].front(); }

    //! Returns the next posting in sorted union order.
    Posting next_posting()
    {
        unsigned int term = heap_[0].term;
        Posting next = ranges_[term].front();
        next.score *= weights_[term];
        ranges_[term].advance();
        if (!ranges_[term].empty()) {
            heap_.push_back({ranges_[term].front().doc, term});
        }
        std::pop_heap(heap_.begin(), heap_.end());
        heap_.pop_back();
        return next;
    }

    //! Returns the next _accumulated posting_ in sorted union order.
    /*!
     * An accumulated posting is an object of the same type as posting, but its
     * score is a sum of all the partial scores for the next available document.
     *
     * Note that if _next_posting_ is invoked independently, this will either
     * return an incorrect score or will skip a document (or documents).
     */
    Posting next_doc()
    {
        auto next = next_posting();
        auto min_doc = next.doc;
        while (!heap_.empty() && heap_[0].doc == min_doc) {
            next.score += next_posting().score;
        }
        return next;
    }

    //! Returns the docterm entries for all terms preceding (and including)
    //! pivot.
    std::vector<docterm> select_pivot(Score threshold)
    {
        Score sum_max_scores(0);
        std::vector<docterm> preceding;
        while (!heap_.empty()) {
            std::pop_heap(heap_.begin(), heap_.end());
            preceding.push_back(heap_.back());
            auto term = preceding.back().term;
            sum_max_scores += max_scores_[term] * weights_[term];
            heap_.pop_back();
            if (sum_max_scores >= threshold) {
                break;
            }
        }
        auto pivot_doc = preceding.back().doc;
        while (!heap_.empty() && peek_posting().doc == pivot_doc) {
            std::pop_heap(heap_.begin(), heap_.end());
            preceding.push_back(heap_.back());
            heap_.pop_back();
        }
        return preceding;
    }

    Posting next_doc_wand(Score threshold)
    {
        while (true) {
            auto preceding = select_pivot(threshold);
            auto pivot_doc = preceding.back().doc;
            if (preceding.front().doc == pivot_doc) {
                Score score(0);
                for (auto & [doc, term] : preceding) {
                    score += ranges_[term].front().score;
                    ranges_[term].advance();
                    if (!ranges_[term].empty()) {
                        heap_.push_back({doc, term});
                        std::push_heap(heap_.begin(), heap_.end());
                    }
                }
                return {pivot_doc, score};
            } else {
                // Move
                for (auto & [doc, term] : preceding) {
                    nextge(term, pivot_doc);
                    if (!ranges_[term].empty()) {
                        heap_.push_back({doc, term});
                        std::push_heap(heap_.begin(), heap_.end());
                    }
                }
            }
        }
    }

    //! Returns true if no further postings are available.
    bool empty() const { return heap_.empty(); }
};

//! Returns top-*k* results, given vectors of posting lists and term weights.
template<class Range, class Score>
std::vector<pure_element_t<Range>> daat_or(
    const std::vector<Range>& query_postings,
    std::size_t k,
    const std::vector<Score>& weights)
{
    using Posting = pure_element_t<Range>;
    UnionRange union_range(query_postings, weights);
    TopKAccumulator<Posting> topk(k);
    while (!union_range.empty()) {
        topk.accumulate(union_range.next_doc());
    }
    return topk.sorted();
}

//! Returns top-*k* results, given vectors of posting lists and term weights.
template<class Range, class Score>
std::vector<pure_element_t<Range>> wand(
    const std::vector<Range>& query_postings,
    std::size_t k,
    const std::vector<Score>& weights)
{
    using Posting = pure_element_t<Range>;
    UnionRange union_range(query_postings, weights);
    TopKAccumulator<Posting> topk(k);
    while (!union_range.empty()) {
        topk.accumulate(union_range.next_doc_wand(topk.threshold()));
    }
    return topk.sorted();
}

};  // namespace irkit

