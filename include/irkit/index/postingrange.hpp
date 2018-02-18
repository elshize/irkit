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

//! \file index.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <irkit/score.hpp>
#include <boost/concept/assert.hpp>
#include <boost/concept_check.hpp>

namespace irk {

//! A posting range with scores calculated on the fly.
/*!
    \tparam Posting  The posting structure type: must have `Posting::doc` and
                     `Posting::score` members.
    \tparam Freq     An integral type of term frequencies.
    \tparam ScoreFn  A scoring function or structure

    \author Michal Siedlaczek
 */
template<class Posting, class Freq, class Scorer = score::tf_idf_scorer>
class dynamically_scored_posting_range {
    BOOST_CONCEPT_ASSERT((boost::Integer<Freq>));

public:
    using posting_type = Posting;
    using document_type = doc_t<posting_type>;
    using score_type = score_t<posting_type>;
    using freqency_type = Freq;
    using scorer_type = Scorer;

private:
    std::vector<document_type> docs_;
    std::vector<freqency_type> counts_;
    freqency_type term_df_;
    std::size_t n_;
    scorer_type score_fn_;

public:
    //! Constructs a posting range for a term.
    /*!
        \param docs      Document IDs.
        \param counts    Occurrences of the term in respective documents.
        \param term_df   The term's document frequency.
        \param n         The collection size.
        \param score_fn  The scoring function or structure of the structure:
                         (freqency_type tf, freqency_type df, std::size_t n) ->
                         score_t<posting_type>
     */
    dynamically_scored_posting_range(std::vector<document_type>&& docs,
        std::vector<Freq>&& counts,
        Freq term_df,
        std::size_t n,
        scorer_type score_fn = score::tf_idf_scorer{})
        : docs_(docs),
          counts_(counts),
          term_df_(term_df),
          n_(n),
          score_fn_(score_fn)
    {}

    class iterator {
        typename std::vector<document_type>::const_iterator doc_iter_;
        typename std::vector<Freq>::const_iterator tf_iter_;
        Freq df_;
        std::size_t n_;
        scorer_type score_fn_;

    public:
        iterator(typename std::vector<document_type>::const_iterator doc_iter,
            typename std::vector<Freq>::const_iterator tf_iter,
            Freq df,
            std::size_t n,
            scorer_type score_fn)
            : doc_iter_(doc_iter),
              tf_iter_(tf_iter),
              df_(df),
              n_(n),
              score_fn_(score_fn)
        {}
        bool operator==(const iterator& rhs) const
        {
            return doc_iter_ == rhs.doc_iter_;
        }
        bool operator!=(const iterator& rhs) const
        {
            return doc_iter_ != rhs.doc_iter_;
        }
        void operator++()
        {
            doc_iter_++;
            tf_iter_++;
        }
        void operator++(int)
        {
            doc_iter_++;
            tf_iter_++;
        }
        auto operator*() const
        {
            score_type score = score_fn_(*tf_iter_, df_, n_);
            return posting_type{*doc_iter_, score};
        }
    };
    iterator cbegin() const
    {
        return iterator{
            docs_.begin(), counts_.begin(), term_df_, n_, score_fn_};
    }
    iterator cend() const
    {
        return iterator{docs_.end(), counts_.end(), term_df_, n_, score_fn_};
    }
    iterator begin() const { return cbegin(); }
    iterator end() const { return cend(); }
    std::size_t size() { return docs_.size(); }
};

};  // namespace irk
