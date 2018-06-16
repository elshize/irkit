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

//! \file score.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <optional>
#include <cmath>
#include <range/v3/utility/concepts.hpp>
#include "irkit/types.hpp"

//! Scoring functions and utilities.
namespace irk::score {

struct scoring_function_tag {
    virtual operator std::string() = 0;
};

struct bm25_tag : public scoring_function_tag {
    operator std::string() override { return "bm25"; }
};

struct query_likelihood_tag : public scoring_function_tag {
    operator std::string() override { return "ql"; }
};

template<class ScoreFn, class Doc, class Freq>
using score_result_t = decltype(std::declval<ScoreFn>()(
    std::declval<Freq>(), std::declval<Freq>(), std::declval<std::size_t>()));

//! A scorer using a simple version of tf-idf function.
struct tf_idf_scorer {
    //! Calculates a simple tf-idf score.
    /*!
     * @tparam Freq An integral type.
     * @param tf    Term frequency in the document being scored.
     * @param df    The term's document frequency in the collection, i.e.,
     *              how many documents include the term.
     */
    template<class Freq, CONCEPT_REQUIRES_(ranges::Integral<Freq>())>
    double operator()(Freq tf, Freq df, std::size_t N) const
    {
        return static_cast<double>(tf)
            * std::log(N / (1 + static_cast<double>(df)));
    }
};

//! A scorer counting term frequencies within scored documents.
struct count_scorer {
    //! Returns the term frequency.
    template<class Freq, CONCEPT_REQUIRES_(ranges::Integral<Freq>())>
    Freq operator()(Freq tf, Freq df, std::size_t N) const
    {
        return tf;
    }
};

//! A BM25 scorer.
struct bm25_scorer {
    using tag_type = bm25_tag;
    tag_type scoring_tag;
    double x, y, z;

    bm25_scorer(long term_document_count,
        long collection_document_count,
        double avg_document_size,
        double k1 = 1.2,
        double b = 0.5)
    {
        double numerator = collection_document_count - term_document_count
            + 0.5;
        double denominator = term_document_count + 0.5;
        double idf = std::log(numerator / denominator);
        x = idf * (k1 + 1);
        y = k1 - b * k1;
        z = b * k1 / avg_document_size;
    }


    //! Returns the BM25 score.
    double operator()(long tf, long document_size) const
    {
        return tf;
    }
};

//! A query likelihood scorer.
struct query_likelihood_scorer {
    using tag_type = query_likelihood_tag;
    tag_type scoring_tag;
    double mu;
    double global_component;

    query_likelihood_scorer(
        long term_occurrences, long all_occurrences, double mu = 2500)
        : mu(mu), global_component(mu * term_occurrences * all_occurrences)
    {}

    double operator()(long tf, long document_size) const
    {
        return (tf + global_component) / (document_size + mu);
    }
};

};  // namespace irk::score
