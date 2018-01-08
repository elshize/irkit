/**!
 * @author Michal Siedlaczek <michal.siedlaczek@nyu.edu>
 */
#pragma once

#include <algorithm>
#include <ostream>
#include <range/v3/utility/concepts.hpp>
#include <vector>
#include "irkit/types.hpp"

namespace irkit {

//! Computes the number of bits required to store an integer n.
/*!
 * @tparam T An integral type
 * @param n The integer of which to return the size in bits.
 */
template<typename T, CONCEPT_REQUIRES_(ranges::Integral<T>())>
constexpr unsigned short nbits(T n)
{
    unsigned short bits = 0;
    while (n >>= 1)
        ++bits;
    return bits;
}

//! An container accumulating top-k postings (or results).
template<class Posting>
class TopKAccumulator {
    using Score = score_t<Posting>;

private:
    std::size_t k_;
    Score threshold_;
    std::vector<Posting> top_;

    static bool result_order(const Posting& lhs, const Posting& rhs)
    {
        return lhs.score > rhs.score;
    };

public:
    //! Initilizes an empty accumulator.
    /*!
     * @param k The size of the accumulator, i.e., the number of postings to
     *          accumulate.
     */
    TopKAccumulator(std::size_t k) : k_(k), threshold_(0){};

    //! Accumulates the given posting.
    /*!
     * If `posting.score` is higher than threshold(), `posting` is accumulated.
     * Furthermore, if the container grows beyond `k`, the lowest scoring
     * posting is discarded.
     */
    void accumulate(Posting posting)
    {
        if (posting.score > threshold_) {
            top_.push_back(posting);
            if (top_.size() <= k_) {
                std::push_heap(top_.begin(), top_.end(), result_order);
            } else {
                std::pop_heap(top_.begin(), top_.end(), result_order);
                top_.pop_back();
            }
            threshold_ = top_.size() == k_ ? top_[0].score : threshold_;
        }
    }

    //! Produces the sorted list of the accumulated postings.
    /*!
     * @returns The list of postings that have been accumulated so far, in order
     *          of decreasing scores.
     */
    std::vector<Posting> sorted()
    {
        std::vector<Posting> sorted = top_;
        std::sort(sorted.begin(), sorted.end(), result_order);
        return sorted;
    }

    //! Returns the current top-k threshold.
    /*!
     * @returns The minimum score to make it to the top-k results; it is either
     *          the score of the k-th result, or 0 if fewer than k results have
     *          been accumulated.
     */
    Score threshold() { return threshold_; }
};

namespace view {

    //! A zip view of two ranges.
    /*!
     * @tparam ZipFn A function type for element generation.
     * @tparam LeftRange The type of the left range.
     * @tparam RightRange The type of the right range.
     */
    template<class ZipFn, class LeftRange, class RightRange>
    class ZipView {
        const LeftRange& left_;
        const RightRange& right_;
        ZipFn zip_fn_;

    public:
        class const_iterator {
            const_iterator_t<LeftRange> liter_;
            const_iterator_t<RightRange> riter_;
            ZipFn zip_fn_;

        public:
            const_iterator(const_iterator_t<LeftRange> liter,
                const_iterator_t<RightRange> riter,
                ZipFn zip_fn)
                : liter_(liter), riter_(riter), zip_fn_(zip_fn)
            {}
            bool operator==(const const_iterator& rhs)
            {
                return liter_ == rhs.liter_ && riter_ == rhs.riter_;
            }
            bool operator!=(const const_iterator& rhs)
            {
                return liter_ != rhs.liter_ || riter_ != rhs.riter_;
            }
            void operator++()
            {
                liter_++;
                riter_++;
            }
            void operator++(int)
            {
                liter_++;
                riter_++;
            }
            auto operator*() { return zip_fn_(*liter_, *riter_); }
        };

        //! Constructs a zip view.
        /*!
         * @param left    The left range.
         * @param right   The right range.
         * @param zip_fn  A function which, given an element of `left` and an
         *                element of `right`, returns an element of the zipped
         *                view.
         *
         * Instead of using the constructor directly, rather use zip() function.
         */
        ZipView(const LeftRange& left, const RightRange& right, ZipFn zip_fn)
            : left_(left), right_(right), zip_fn_(zip_fn)
        {}
        const_iterator cbegin() const
        {
            return const_iterator{left_.cbegin(), right_.cbegin(), zip_fn_};
        }
        const_iterator cend() const
        {
            return const_iterator{left_.cend(), right_.cend(), zip_fn_};
        }
        const_iterator begin() const { return cbegin(); }
        const_iterator end() const { return cend(); }
    };

    //! Constructs a zip view.
    /*!
     * @tparam ZipFn A function type for element generation.
     * @tparam LeftRange The type of the left range.
     * @tparam RightRange The type of the right range.
     *
     * @param left    The left range.
     * @param right   The right range.
     * @param zip_fn  A function which, given an element of `left` and an
     *                element of `right`, returns an element of the zipped
     *                view.
     *
     * Example:
     * @code
     *   std::vector<int> documents = {0, 1, 3};
     *   std::vector<double> scores = {1.2, 3.1, 0.4};
     *   auto zip_view = zip(documents, scores,
     *     [](const int& doc, const double& score) {
     *       return std::make_pair(doc, score);
     *     });
     *   for (const auto& [doc, score] : zip_view) {
     *     std::cout << doc << ", " << score << std::endl;
     *   }
     * @endcode
     */
    template<class ZipFn, class LeftRange, class RightRange>
    auto zip(LeftRange left, RightRange right, ZipFn zip_fn)
    {
        return ZipView(left, right, zip_fn);
    }

    //! Constructs a zip view of Posting elements.
    /*!
     * It is a specialization of zip() function, which always returns elements
     * of type `Posting`.
     *
     * @tparam Posting Any structure or class that can be constructed with
     *         `Posting{doc, score}`.
     * @tparam LeftRange The type of the left range.
     * @tparam RightRange The type of the right range.
     */
    template<class Posting, class LeftRange, class RightRange>
    auto posting_zip(LeftRange left, RightRange right)
    {
        return ZipView(left, right, [](const auto& doc, const auto& score) {
            return Posting{doc, score};
        });
    }

};  // namespace view

}  // namespace irkit
