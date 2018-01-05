#pragma once

#include <iostream>
#include <optional>
#include <type_traits>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/core.hpp>
#include <range/v3/numeric/accumulate.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/group_by.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>
#include "irkit/heap.hpp"

namespace irkit {


};  // namespace irkit

namespace irkit::view {

template<class Rng>
using range_value_type = ranges::range_value_type_t<Rng>;
template<class Rngs>
using range_range_value_type = range_value_type<range_value_type<Rngs>>;

template<class Rngs>
class union_merge_view
    : public ranges::view_facade<union_merge_view<Rngs>> {
    friend ranges::range_access;
    std::vector<ranges::range_value_type_t<Rngs>> rngs_;
    struct cursor {
        using range_type = ranges::range_value_type_t<Rngs>;
        using element_type = ranges::range_value_type_t<range_type>;
        std::vector<range_type>* rngs_;
        std::vector<ranges::iterator_t<range_type>> its_;
        std::vector<ranges::sentinel_t<range_type>> ends_;
        Heap<element_type, std::size_t> heap_;
        std::size_t n_;
        cursor() = default;
        cursor(std::vector<range_type>* rngs,
            std::vector<ranges::iterator_t<range_type>> its,
            std::vector<ranges::sentinel_t<range_type>> ends)
            : rngs_(rngs), its_(its), ends_(ends), n_(0)
        {
            //auto front =
            //    ranges::view::transform(its_, [=](auto it) { return *it; });
            auto indices = ranges::view::iota(0u, rngs_->size());
            auto zipped = ranges::view::zip(indices, its_, ends_);
            ranges::for_each(zipped, [=](auto tup) {
                auto[idx, beg, end] = tup;
                if (beg != end) {
                    heap_.push(*beg, idx);
                }
            });
        }
        element_type read() const { return *its_[heap_.top().value]; }
        void next()
        {
            std::size_t idx = heap_.top().value;
            ++its_[idx];
            if (its_[idx] == ends_[idx]) {
                heap_.pop();
            } else {
                heap_.pop_push(*its_[idx], idx);
            }
            ++n_;
        }
        bool equal(ranges::default_sentinel) const { return heap_.empty(); }
        CONCEPT_REQUIRES(ranges::ForwardRange<range_type>())
        bool equal(cursor const& that) const {
            return n_ == that.n_;
        }
    };
    cursor begin_cursor()
    {
        return cursor(&rngs_,
            ranges::view::transform(rngs_, ranges::begin),
            ranges::view::transform(rngs_, ranges::end));
    }

public:
    union_merge_view() = default;
    explicit union_merge_view(Rngs rngs) : rngs_(std::move(rngs)) {}
};

/// In:  Range<Range<T>>, where the inner ranges are sorted.
/// Out: Range<T>, which is a sorted union of the input ranges.
auto union_merge()
{
    return ranges::make_pipeable([](auto&& rngs) {
        using Rngs = decltype(rngs);
        return union_merge_view<ranges::view::all_t<Rngs>>(
            ranges::view::all(std::forward<Rngs>(rngs)));
    });
}

/// In:  Range<T>, where the elements are sorted.
/// Out: Range<Range<T>>, a sorted range of groups of equal elements according
///                       to `equal_to`.
template<class Fun = ranges::equal_to>
auto group_sorted(Fun equal_to = ranges::equal_to{})
{
    return ranges::view::group_by(equal_to);
}

/// In:  Range<Range<T>>, a sorted range of groups.
/// Out: Range<T>, where each element is a sum of the corresponding group.
template<class Fun = ranges::plus>
auto accumulate_groups(Fun add = ranges::plus{})
{
    return ranges::view::transform([=](auto rng) {
        return ranges::accumulate(
            rng | ranges::view::drop(1), ranges::front(rng), add);
    });
}

/// In:  Range<T>, where the elements are sorted.
/// Out: Range<T>, where each element is a sum of the consecutive elements that
///                are equal according to `equal_to`.
template<class Equals = ranges::equal_to, class Add = ranges::plus>
auto accumulate_sorted(
    Equals equal_to = ranges::equal_to{}, Add add = ranges::plus{})
{
    return group_sorted(equal_to) | accumulate_groups(add);
}

struct greater {
    template<typename T,
        typename U,
        CONCEPT_REQUIRES_(ranges::WeaklyOrdered<T, U>())>
    constexpr bool operator()(T&& t, U&& u) const
    {
        return (T &&) t > (U &&) u;
    }
    using is_transparent = void;
};

/// In:  Range<T>
/// Out: Range<T>: top k elements according to compare (sorted)
template<class Compare = greater>
auto top_k(std::size_t k, Compare compare = greater{})
{
    return ranges::make_pipeable([=](auto&& rng) {
        using Rng = decltype(rng);
        using Val = range_value_type<Rng>;
        std::vector<Val> heap;
        for (auto element : rng) {
            if (heap.size() < k) {
                heap.push_back(element);
                std::push_heap(heap.begin(), heap.end(), compare);
            } else {
                if (compare(element, heap[0])) {
                    std::pop_heap(heap.begin(), heap.end(), compare);
                    heap[heap.size() - 1] = element;
                    std::push_heap(heap.begin(), heap.end(), compare);
                }
            }
        }
        std::sort(heap.begin(), heap.end(), compare);
        return heap;
    });
}

/// In:  Range<T>
/// Out: Range<T>, where each element is multiplied by `weight`.
template<class W, class Multiply = ranges::multiplies>
auto weighted(W weight, Multiply multiply = ranges::multiplies{})
{
    return ranges::view::transform(
        [=](auto element) { return multiply(element, weight); });
}








template<class Rng>
class any_range {
private:
    Rng rng_;

public:
    any_range(Rng rng) : rng_(std::move(rng)) {}
    auto begin() { return rng_.begin(); }
    auto end() { return rng_.end(); }
};

template<class Iter>
class iterator_range {
private:
    Iter begin_;
    Iter end_;

public:
    iterator_range(Iter first, Iter last) : begin_(first), end_(last) {}
    Iter begin() { return begin_; }
    Iter end() { return end_; }
};

template<class Rng>
auto to_vector(Rng rng)
{
    auto it = rng.begin();
    auto end = rng.end();
    using Val = std::remove_const_t<std::remove_reference_t<decltype(*it)>>;
    std::vector<Val> vec;
    for (; it != end; ++it) {
        Val val = *it;
        //std::cout << val << '\n';
        vec.push_back(val);
    }
    return vec;
}

///// In:  Range<T>
///// Out: Range<T>, where each element is multiplied by `weight`.
//template<class W, class Multiply = ranges::multiplies>
//auto weighted(W weight, Multiply multiply = ranges::multiplies{})
//{
//    return ranges::view::transform(
//        [=](auto element) { return multiply(element, weight); });
//}

template<class Rng, class Fun>
class transform_view {
private:
    Rng rng_;
    Fun fun_;
    struct iterator {
        typename Rng::iterator it_;
        Fun fun_;
        iterator() = default;
        iterator(typename Rng::iterator it, Fun fun) : it_(it), fun_(fun) {}
        auto operator*() { return fun_(*it_); }
        iterator& operator++()
        {
            it_++;
            return *this;
        }
        iterator operator++(int)
        {
            return iterator{it_++, fun_};
        }
        bool operator==(iterator rhs) const
        {
            return it_ == rhs.it_;
        }
        bool operator!=(iterator rhs) const
        {
            return it_ != rhs.it_;
        }
    };

public:
    transform_view(Rng rng, Fun fun) : rng_(std::move(rng)), fun_(fun) {}
    iterator begin() { return iterator{rng_.begin(), fun_}; }
    iterator end() { return iterator{rng_.end(), fun_}; }
};

//template<class Rng, class Fun>
//class take_while_view {
//private:
//    Rng rng_;
//    Fun fun_;
//    struct iterator;
//    struct sentinel : public iterator {
//        sentinel(typename Rng::iterator it,
//            typename Rng::iterator end,
//            Fun fun,
//            bool sentinel)
//            : iterator(it, end, fun, sentinel)
//        {}
//    };
//    struct iterator {
//        typename Rng::iterator it_;
//        typename Rng::iterator end_;
//        Fun fun_;
//        bool sentinel_;
//        iterator() = default;
//        iterator(typename Rng::iterator it, Fun fun, bool sentinel)
//            : it_(it), fun_(fun), sentinel_(sentinel)
//        {}
//        auto operator*() { return *it_; }
//        iterator& operator++()
//        {
//            it_++;
//            return *this;
//        }
//        iterator operator++(int) { return iterator{it_++, fun_, sentinel_}; }
//        bool operator==(iterator rhs) const
//        {
//            return it_ == rhs.it_ || (rhs.sentinel_ && !fun(*it_));
//        }
//        bool operator!=(iterator rhs) const
//        {
//            return it_ != rhs.it_ && fun_(*it_);
//        }
//    };
//
//public:
//    take_while_view(Rng rng, Fun fun) : rng_(std::move(rng)), fun_(fun) {}
//    iterator begin() { return iterator{rng_.begin(), rng_.end(), fun_, false}; }
//    iterator end() { return sentinel{rng_.end(), rng_.end(), fun_, true}; }
//};

template<class Rngs>
// Rngs is a Random Access Range
class fast_union_merge_view {
private:
    Rngs rngs_;
    using Rng =
        std::remove_const_t<std::remove_reference_t<decltype(rngs_[0])>>;
    using Iter = decltype(rngs_[0].begin());
    using T = std::remove_const_t<
        std::remove_reference_t<decltype(*rngs_[0].begin())>>;
    struct iterator {
        Heap<T, std::size_t> heap_;
        std::vector<Iter> its_;
        std::vector<Iter> ends_;
        std::size_t n_;
        iterator() = default;
        iterator(std::vector<Iter> its, std::vector<Iter> ends, std::size_t n)
            : its_(std::move(its)), ends_(std::move(ends)), n_(n)
        {
            for (std::size_t idx = 0; idx < its_.size(); ++idx) {
                if (its_[idx] != ends_[idx]) {
                    heap_.push(*its_[idx], idx);
                }
            }
        }
        void next()
        {
            std::size_t idx = heap_.top().value;
            ++its_[idx];
            if (its_[idx] == ends_[idx]) {
                heap_.pop();
            } else {
                heap_.pop_push(*its_[idx], idx);
            }
            n_++;
        }
        T operator*() { return *its_[heap_.top().value]; }
        iterator& operator++()
        {
            next();
            return *this;
        }
        iterator& operator++(int)
        {
            next();
            return *this;
        }
        //iterator operator+(int n) const { return {posting_list, pos + n}; }
        bool operator==(iterator rhs) const
        {
            return heap_.size() == rhs.heap_.size() && n_ == rhs.n_;
        }
        bool operator!=(iterator rhs) const
        {
            return heap_.size() != rhs.heap_.size()
                || (!heap_.empty() && n_ != rhs.n_);
        }
    };

public:
    fast_union_merge_view(Rngs rngs) : rngs_(rngs) {}
    iterator begin() {
        auto len = rngs_.size();
        std::vector<Iter> its_;
        std::vector<Iter> ends_;
        for (std::size_t idx = 0; idx < len; ++idx) {
            its_.push_back(rngs_[idx].begin());
            ends_.push_back(rngs_[idx].end());
        }
        return iterator{std::move(its_), std::move(ends_), 0};
    }
    iterator end() {
        return iterator{{}, {}, std::numeric_limits<std::size_t>::max()};
    }
};

template<class Rng, class Equals, class Aggregate>
class aggregate_sorted_view {
private:
    Rng rng_;
    using T =
        std::remove_const_t<std::remove_reference_t<decltype(*rng_.begin())>>;
    using Iter =
        std::remove_const_t<std::remove_reference_t<decltype(rng_.begin())>>;
    Equals equals_;
    Aggregate aggregate_;
    struct iterator {
        Iter it_;
        Iter end_;
        Equals equals_;
        Aggregate aggregate_;
        std::optional<T> current_;
        iterator() = default;
        iterator(Iter it, Iter end, Equals equals, Aggregate aggregate)
            : it_(it),
              end_(end),
              equals_(equals),
              aggregate_(aggregate),
              current_(std::nullopt)
        {}
        T operator*()
        {
            if (!current_.has_value()) {
                T first = *it_;
                T val = first;
                ++it_;
                while (it_ != end_ && equals_(first, *it_)) {
                    val = aggregate_(val, *it_);
                    ++it_;
                }
                current_ = std::optional<T>(val);
            }
            return current_.value();
        }
        iterator& operator++()
        {
            if (current_.has_value()) {
                current_ = std::nullopt;
            } else if (it_ != end_) {
                auto head = *it_;
                while (it_ != end_ && equals_(head, *it_)) {
                    ++it_;
                }
            }
            return *this;
        }
        iterator& operator++(int) { return ++(*this); }
        bool operator==(iterator rhs) const
        {
            return it_ == rhs.it_;
        }
        bool operator!=(iterator rhs) const
        {
            return it_ != rhs.it_;
        }
    };

public:
    aggregate_sorted_view(Rng rng, Equals equals, Aggregate aggregate)
        : rng_(std::move(rng)), equals_(equals), aggregate_(aggregate)
    {}
    iterator begin()
    {
        return iterator{rng_.begin(), rng_.end(), equals_, aggregate_};
    }
    iterator end()
    {
        return iterator{rng_.end(), rng_.end(), equals_, aggregate_};
    }
};

template<class Rng, class Compare = greater>
auto top_k(Rng rng, std::size_t k, Compare compare = greater{})
{
    using T =
        std::remove_const_t<std::remove_reference_t<decltype(*rng.begin())>>;
    std::vector<T> heap;
    for (auto element : rng) {
        if (heap.size() < k) {
            heap.push_back(element);
            std::push_heap(heap.begin(), heap.end(), compare);
        } else {
            if (compare(element, heap[0])) {
                std::pop_heap(heap.begin(), heap.end(), compare);
                heap[heap.size() - 1] = element;
                std::push_heap(heap.begin(), heap.end(), compare);
            }
        }
    }
    std::sort(heap.begin(), heap.end(), compare);
    return heap;
}

};  // namespace irkit::view

