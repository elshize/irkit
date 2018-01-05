#pragma once

#include <algorithm>
#include <vector>
#include "irkit/types.hpp"

namespace irkit {

//! Computes the number of bits required to store an integer n.
template<typename Integer>
constexpr unsigned short nbits(Integer n)
{
    unsigned short bits = 0;
    while (n >>= 1)
        ++bits;
    return bits;
}

//! Accumulates top-k postings (or results).
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
    TopKAccumulator(std::size_t k) : k_(k), threshold_(0){};
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
    std::vector<Posting> sorted()
    {
        std::vector<Posting> sorted = top_;
        std::sort(sorted.begin(), sorted.end(), result_order);
        return sorted;
    }
    Score threshold() { return threshold_; }
};

namespace view {

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

    template<class ZipFn, class LeftRange, class RightRange>
    auto zip(LeftRange left, RightRange right, ZipFn zip_fn)
    {
        return ZipView(left, right, zip_fn);
    }

    template<class Posting, class LeftRange, class RightRange>
    auto posting_zip(LeftRange left, RightRange right)
    {
        return ZipView(left, right, [](const auto& doc, const auto& score) {
            return Posting{doc, score};
        });
    }

};  // namespace view

}  // namespace irkit
