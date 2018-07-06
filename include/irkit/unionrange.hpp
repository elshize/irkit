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

//! \file unionrange.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <algorithm>
#include "irkit/movingrange.hpp"
#include "irkit/types.hpp"
#include "irkit/utils.hpp"

namespace irk {

//! Represents a union of sorted ranges (e.g., posting lists).
/*!
    TODO:
        - Generalize.
        - Improve efficiency below 17 ms on BoVW.
 */
template<class Range>
class union_range {
public:
    using range_type = Range;
    using posting_type = pure_element_t<range_type>;
    using score_type = score_t<posting_type>;
    using doc_type = doc_t<posting_type>;

protected:
    //! Pair of document ID and term ID to use in DAAT term heap.
    struct docterm {
        doc_type doc;
        unsigned int term;
        docterm(doc_type d, unsigned int t) : doc(d), term(t) {}
        bool operator==(const docterm& rhs) const
        {
            return doc == rhs.doc && term == rhs.term;
        }
        bool operator<(const docterm& rhs) const { return doc > rhs.doc; }
    };

    const std::vector<score_type>& weights_;
    const std::vector<score_type>& max_scores_;
    std::vector<moving_range<const_iterator_t<range_type>>> ranges_;
    std::vector<docterm> heap_;

    //! Initilizes moving ranges and heap.
    void init(const std::vector<range_type>& query_postings)
    {
        for (auto& postinglist : query_postings) {
            ranges_.emplace_back(postinglist.cbegin(), postinglist.cend());
        }
        for (unsigned int term = 0; term < ranges_.size(); ++term) {
            if (not ranges_[term].empty()) {
                heap_.push_back({ranges_[term].front().doc, term});
            }
        }
        std::make_heap(heap_.begin(), heap_.end());
    }

    void nextge(unsigned int term, doc_type doc)
    {
        while (ranges_[term].front().doc < doc) {
            ranges_[term].advance();
        }
    }

public:
    //! Creates new union range from posting lists and term weights.
    /*!
        \pre Each posting list must be sorted by increasing document IDs.
        \param query_postings   a vector of posting lists for all query terms
        \param weights          a vector of term weights
     */
    union_range(const std::vector<range_type>& query_postings,
        const std::vector<score_type>& weights,
        const std::vector<score_type>& max_scores = std::move(
            std::vector<score_type>()))
        : weights_(weights), max_scores_(max_scores)
    {
        init(query_postings);
    }

    //! Returns the next posting's term without advancing to the next one.
    unsigned int peek_term() { return heap_[0].term; }

    //! Returns the next posting without advancing to the next one.
    posting_type peek_posting() { return ranges_[heap_[0].term].front(); }

    //! Returns the next posting in sorted union order.
    posting_type next_posting()
    {
        unsigned int term = heap_[0].term;
        posting_type next = ranges_[term].front();
        next.score *= weights_[term];
        ranges_[term].advance();
        if (not ranges_[term].empty()) {
            heap_.push_back({ranges_[term].front().doc, term});
        }
        std::pop_heap(heap_.begin(), heap_.end());
        heap_.pop_back();
        return next;
    }

    //! Returns the next _accumulated posting_ in sorted union order.
    /*!
        An accumulated posting is an object of the same type as posting, but its
        score is a sum of all the partial scores for the next available
        document.

        Note that if _next_posting_ is invoked independently, this will either
        return an incorrect score or will skip a document (or documents).
     */
    posting_type next_doc()
    {
        auto next = next_posting();
        auto min_doc = next.doc;
        while (not heap_.empty() && heap_[0].doc == min_doc) {
            next.score += next_posting().score;
        }
        return next;
    }

    //! Returns the docterm entries for all terms preceding (and including)
    //! pivot.
    std::vector<docterm> select_pivot(score_type threshold)
    {
        score_type sum_max_scores(0);
        std::vector<docterm> preceding;
        while (not heap_.empty()) {
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
        while (not heap_.empty() && peek_posting().doc == pivot_doc) {
            std::pop_heap(heap_.begin(), heap_.end());
            preceding.push_back(heap_.back());
            heap_.pop_back();
        }
        return preceding;
    }

    posting_type next_doc_wand(score_type threshold)
    {
        while (true) {
            auto preceding = select_pivot(threshold);
            auto pivot_doc = preceding.back().doc;
            if (preceding.front().doc == pivot_doc) {
                score_type score(0);
                for (auto & [doc, term] : preceding) {
                    score += ranges_[term].front().score;
                    ranges_[term].advance();
                    if (not ranges_[term].empty()) {
                        heap_.push_back({doc, term});
                        std::push_heap(heap_.begin(), heap_.end());
                    }
                }
                return {pivot_doc, score};
            } else {
                // Move
                for (auto & [doc, term] : preceding) {
                    nextge(term, pivot_doc);
                    if (not ranges_[term].empty()) {
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

};  // namespace irk
