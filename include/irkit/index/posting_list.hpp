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

//! \file documentlist.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <boost/iterator/iterator_facade.hpp>

#include <irkit/assert.hpp>

namespace irk {

// TODO(michal): Make variable numbers of payloads
template<class DocumentListT, class PayloadListT>
class posting_list_view {
public:
    using document_list_type = DocumentListT;
    using payload_list_type = PayloadListT;
    using document_type = typename document_list_type::value_type;
    using payload_type = typename payload_list_type::value_type;
    using difference_type = std::int32_t;
    using document_iterator_t = typename document_list_type::iterator;
    using payload_iterator_t = typename payload_list_type::iterator;

    class posting_view {
    public:
        posting_view(const document_iterator_t& document_iterator,
            const payload_iterator_t& payload_iterator)
            : document_iterator_(document_iterator),
              payload_iterator_(payload_iterator)
        {}
        const document_type& document() const { return *document_iterator_; }
        const payload_type& payload() const { return *payload_iterator_; }
        explicit operator std::pair<document_type, payload_type>() const
        {
            return std::pair(document(), payload());
        }
        explicit operator std::tuple<document_type, payload_type>() const
        {
            return std::pair(document(), payload());
        }

    private:
        const document_iterator_t& document_iterator_;
        const payload_iterator_t& payload_iterator_;
    };

    class iterator : public boost::iterator_facade<iterator,
                         posting_view,
                         boost::forward_traversal_tag,
                         posting_view> {
    public:
        iterator(document_iterator_t document_iterator,
            payload_iterator_t payload_iterator)
            : document_iterator_(std::move(document_iterator)),
              payload_iterator_(std::move(payload_iterator)),
              current_posting_(document_iterator_, payload_iterator_)
        {}
        iterator(const iterator& other)
            : document_iterator_(other.document_iterator_),
              payload_iterator_(other.payload_iterator_),
              current_posting_(document_iterator_, payload_iterator_)
        {}
        iterator(iterator&&) = default;  // NOLINT
        iterator& operator=(const iterator&) = default;
        iterator& operator=(iterator&&) = default;  // NOLINT
        ~iterator() = default;

        iterator& moveto(document_type doc)
        {
            document_iterator_.moveto(doc);
            payload_iterator_.align(document_iterator_);
            return *this;
        }

        iterator nextgeq(document_type doc) const
        {
            iterator iter(*this);
            iter.moveto(doc);
            return iter;
        }

        document_type document() const { return *document_iterator_; }
        payload_type payload() const { return *payload_iterator_; }

    private:
        friend class boost::iterator_core_access;
        void increment()
        {
            ++document_iterator_;
            ++payload_iterator_;
        }
        void advance(difference_type n)
        {
            document_iterator_ += n;
            payload_iterator_ += n;
        }
        bool equal(const iterator& other) const
        {
            return document_iterator_ == other.document_iterator_;
        }

        const posting_view& dereference() const { return current_posting_; }

    public:
        document_iterator_t document_iterator_;
        payload_iterator_t payload_iterator_;
        posting_view current_posting_;
    };
    using const_iterator = iterator;

    posting_list_view(document_list_type documents, payload_list_type payloads)
        : documents_(std::move(documents)), payloads_(std::move(payloads))
    { EXPECTS(documents_.size() == payloads_.size()); }

    iterator begin() const
    { return iterator(documents_.begin(), payloads_.begin()); };
    iterator end() const
    { return iterator(documents_.end(), payloads_.end()); };
    iterator lookup(document_type id) const { return begin().nextgeq(id); };
    difference_type size() const { return documents_.size(); }

private:
    document_list_type documents_;
    payload_list_type payloads_;
};

}  // namespace irk
