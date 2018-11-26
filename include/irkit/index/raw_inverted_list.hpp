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
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <iterator>
#include <sstream>
#include <vector>

#include <boost/iterator.hpp>

#include <irkit/coding.hpp>
#include <irkit/coding/stream_vbyte.hpp>
#include <irkit/coding/vbyte.hpp>
#include <irkit/index/block.hpp>
#include <irkit/index/types.hpp>
#include <irkit/memoryview.hpp>
#include <irkit/sgnd.hpp>

namespace irk {

template<class D, class P>
class raw_posting {
    D document_{};
    P payload_{};

public:
    raw_posting() = default;
    raw_posting(D document, P payload) noexcept
        : document_(document), payload_(payload)
    {}
    raw_posting(const raw_posting&) = default;
    raw_posting(raw_posting&&) noexcept = default;
    raw_posting& operator=(const raw_posting&) = default;
    raw_posting& operator=(raw_posting&&) noexcept = default;
    ~raw_posting() = default;

    const D& document() const { return document_; }
    const P& payload() const { return payload_; }
};

template<class Iterator>
class raw_iterator : public boost::iterator_facade<
                         raw_iterator<Iterator>,
                         typename std::iterator_traits<Iterator>::value_type,
                         boost::forward_traversal_tag> {
public:
    using difference_type = std::ptrdiff_t;
    using value_type = typename std::iterator_traits<Iterator>::value_type;
    using reference = value_type&;

    raw_iterator(const raw_iterator&) = default;
    raw_iterator(raw_iterator&&) noexcept = default;
    raw_iterator& operator=(const raw_iterator&) = default;
    raw_iterator& operator=(raw_iterator&&) noexcept = default;
    ~raw_iterator() = default;

    raw_iterator(index::term_id_t term_id, Iterator pos, Iterator end)
        : term_id_(term_id), pos_(std::move(pos)), end_(std::move(end))
    {}

    raw_iterator& moveto(value_type val)
    {
        while (pos_ != end_ && *pos_ < val) {
            ++pos_;
        }
        return *this;
    };

    raw_iterator nextgeq(value_type val)
    {
        raw_iterator next(*this);
        next.moveto(val);
        return next;
    };

    const index::term_id_t& term_id() const { return term_id_; }

private:
    friend class boost::iterator_core_access;
    void increment() { ++pos_; }
    void advance(difference_type n) { std::advance(pos_, n); }
    bool equal(const raw_iterator& other) const { return pos_ == other.pos_; }
    const value_type& dereference() const { return *pos_; }

    void finish() { pos_ = end_; }

    index::term_id_t term_id_{};
    Iterator pos_{};
    Iterator end_{};
};

template<class T>
class raw_inverted_list {
public:
    using size_type = int64_t;
    using value_type = T;

    raw_inverted_list() = default;

    template<class Iterator>
    raw_inverted_list(index::term_id_t term_id, Iterator first, Iterator last)
        : term_id_(term_id), elements_(first, last)
    {}

    template<class Iterator, class UnaryFunction>
    raw_inverted_list(
        index::term_id_t term_id,
        Iterator first,
        Iterator last,
        UnaryFunction f)
        : term_id_(term_id)
    {
        std::transform(first, last, std::back_inserter(elements_), f);
    }

    raw_inverted_list(index::term_id_t term_id, std::vector<T> elements)
        : term_id_(term_id), elements_(std::move(elements))
    {}

    auto begin() const { return elements_.cbegin(); };
    auto end() const { return elements_.cend(); };

    auto lookup(value_type id) const { return begin().nextgeq(id); };
    int64_t size() const { return irk::sgnd(elements_.size()); }
    const index::term_id_t& term_id() const { return term_id_; }

private:
    index::term_id_t term_id_{};
    std::vector<T> elements_;
};

template<class Codec>
using raw_document_list = raw_inverted_list<index::document_t>;

template<class Payload, class Codec>
using raw_payload_list = raw_inverted_list<Payload>;

}  // namespace irk
