#include <cmath>
#include <range/v3/utility/concepts.hpp>
#include "irkit/types.hpp"

namespace irkit::score {

template<class ScoreFn, class Doc, class Freq>
using score_result_t = decltype(std::declval<ScoreFn>()(
    std::declval<Freq>(), std::declval<Freq>(), std::declval<std::size_t>()));

struct TfIdf {
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

struct Count {
    //! Returns the term frequency.
    template<class Freq, CONCEPT_REQUIRES_(ranges::Integral<Freq>())>
    Freq operator()(Freq tf, Freq df, std::size_t N) const
    {
        return tf;
    }
};

struct BM25 {
    double k1;
    double b;

    BM25(double k1 = 1.2, double b = 0.5) {}

    //! Returns the BM25 score.
    template<class Freq, CONCEPT_REQUIRES_(ranges::Integral<Freq>())>
    Freq operator()(Freq tf, Freq df, std::size_t N) const
    {
        return tf;
    }
};

};  // namespace irkit::score
