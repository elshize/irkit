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

//! \file
//! \author     Michal Siedlaczek
//! \copyright  MIT License

#pragma once

#include <algorithm>
#include <cstdint>
#include <ostream>
#include <vector>

#include <range/v3/utility/concepts.hpp>

#include <irkit/types.hpp>

namespace irk {

using std::int16_t;
using std::uint16_t;

//! Computes the number of bits required to store an integer n.
/*!
 * @tparam T An integral type
 * @param n The integer of which to return the size in bits.
 */
template<typename T, CONCEPT_REQUIRES_(ranges::Integral<T>())>
constexpr int16_t nbits(T n)
{
    uint16_t bits = 0;
    while (n >>= 1) { ++bits; }
    return static_cast<int16_t>(bits);
}

//! Computes the number of full bytes required to store an integer n.
/*!
 * @tparam T An integral type
 * @param n The integer of which to return the size in bytes.
 */
template<typename T, CONCEPT_REQUIRES_(ranges::Integral<T>())>
constexpr int16_t nbytes(T n)
{
    auto bits = nbits(n);
    return (bits + 7) / 8;
}

template<typename Range>
auto collect(const Range& range)
{
    using value_type =
        std::remove_const_t<std::remove_reference_t<decltype(*range.begin())>>;
    return std::vector<value_type>(range.begin(), range.end());
}

//! An container accumulating top-k postings (or results).
template<class Key, class Value>
class top_k_accumulator {
public:
    using key_type = Key;
    using value_type = Value;
    using entry_type = std::pair<key_type, value_type>;
private:
    std::size_t k_;
    value_type threshold_ = std::numeric_limits<value_type>::lowest();
    std::vector<entry_type> top_;

    static bool result_order(const entry_type& lhs, const entry_type& rhs)
    {
        return lhs.second > rhs.second;
    };

public:
    //! Initilizes an empty accumulator.
    /*!
     * @param k The size of the accumulator, i.e., the number of postings to
     *          accumulate.
     */
    explicit top_k_accumulator(std::size_t k) : k_(k){};

    //! Accumulates the given posting.
    /*!
     * If `posting.score` is higher than threshold(), `posting` is accumulated.
     * Furthermore, if the container grows beyond `k`, the lowest scoring
     * posting is discarded.
     *
     * \return  `true` if accumulated
     */
    bool accumulate(key_type key, value_type value)
    {
        if (value > threshold_) {
            top_.emplace_back(key, value);
            if (top_.size() <= k_) {
                std::push_heap(top_.begin(), top_.end(), result_order);
            } else {
                std::pop_heap(top_.begin(), top_.end(), result_order);
                top_.pop_back();
            }
            threshold_ = top_.size() == k_ ? top_[0].second : threshold_;
            return true;
        }
        return false;
    }

    //! Produces the sorted list of the accumulated postings.
    /*!
     * @returns The list of postings that have been accumulated so far, in order
     *          of decreasing scores.
     */
    std::vector<entry_type> sorted()
    {
        std::vector<entry_type> sorted = top_;
        std::sort(sorted.begin(), sorted.end(), result_order);
        return sorted;
    }

    const std::vector<entry_type>& unsorted() const { return top_; }

    //! Returns the current top-k threshold.
    /*!
     * @returns The minimum score to make it to the top-k results; it is either
     *          the score of the k-th result, or 0 if fewer than k results have
     *          been accumulated.
     */
    value_type threshold() { return threshold_; }

    auto size() const { return top_.size(); }
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
            bool operator==(const const_iterator& rhs) const
            {
                return liter_ == rhs.liter_ && riter_ == rhs.riter_;
            }
            bool operator!=(const const_iterator& rhs) const
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
            auto operator*() {
                return zip_fn_(*liter_, *riter_);
            }
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
    auto posting_zip(const LeftRange& left, const RightRange& right)
    {
        return ZipView(left, right, [](const auto& doc, const auto& score) {
            return Posting{doc, score};
        });
    }

}  // namespace view

}  // namespace irk
