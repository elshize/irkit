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

//! \file vector_inverted_list.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <vector>

#include <boost/filesystem.hpp>
#include <boost/iterator/iterator_facade.hpp>

namespace irk {

namespace fs = boost::filesystem;

template<class ListT, bool sorted>
class vector_block_iterator : public boost::iterator_facade<
        vector_block_iterator<ListT, sorted>,
        const typename ListT::value_type,
        boost::forward_traversal_tag> {
public:
    using view_type = ListT;
    using self_type = vector_block_iterator<view_type, sorted>;
    using difference_type = int;
    using value_type = typename ListT::value_type;

    vector_block_iterator() = default;
    vector_block_iterator(const self_type&) = default;
    vector_block_iterator(
        const view_type& view, difference_type block, difference_type pos)
        : view_(view), block_(block), pos_(pos)
    {}

    self_type& moveto(value_type id)
    {
        static_assert(
            sorted == true, "moveto not supported for unsorted lists");
        int block = nextgeq_block(id);
        if (block >= view_.num_blocks())
        {
            finish();
            return *this;
        }
        pos_ = block == block_ ? pos_ : 0;
        block_ = block;
        while (view_[absolute_position()] < id) { pos_++; }
        return *this;
    };

    self_type nextgeq(value_type id) const
    {
        static_assert(
            sorted == true, "nextgeq not supported for unsorted lists");
        self_type next(*this);
        next.moveto(id);
        return next;
    }

    template<class BlockIterator>
    self_type& align(const BlockIterator& other)
    {
        block_ = other.block_;
        pos_ = other.pos_;
        return *this;
    }

    template<class BlockIterator>
    self_type aligned(const BlockIterator& other)
    {
        self_type al(*this);
        al.align(other);
        return al;
    }

private:
    friend class boost::iterator_core_access;

    void increment()
    {
        block_ += (pos_ + 1) / view_.block_size();
        pos_ = (pos_ + 1) % view_.block_size();
    }

    void advance(difference_type n)
    {
        block_ += (pos_ + n) / view_.block_size();
        pos_ = (pos_ + n) % view_.block_size();
    }

    bool equal(const self_type& other) const
    { return pos_ == other.pos_ && block_ == other.block_; }

    const value_type& dereference() const { return view_[absolute_position()]; }

    difference_type absolute_position() const
    { return view_.block_size() * block_ + pos_; }

    difference_type absolute_position(int block, difference_type pos) const
    { return view_.block_size() * block + pos; }

    int nextgeq_block(value_type id) const
    {
        int block = block_;
        while (block < view_.num_blocks() && last_in_block(block) < id)
        { block++; }
        return block;
    }

    difference_type last_in_block(int block) const
    {
        auto idx = block < view_.num_blocks() - 1
            ? view_.block_size() - 1
            : (view_.size() - 1) % view_.block_size();
        return view_[absolute_position(block, idx)];
    }

    void finish()
    {
        block_ = view_.num_blocks() - 1;
        pos_ = view_.size() % view_.block_size();
    }

    const view_type& view_;
    std::size_t block_;
    std::size_t pos_;
};

template<class TDoc = long>
class vector_document_list {
public:
    using size_type = long;
    using value_type = TDoc;
    using self_type = vector_document_list<TDoc>;
    using iterator = vector_block_iterator<self_type, true>;
    using const_iterator = iterator;

    vector_document_list(std::vector<value_type> vec)
        : ids_(std::move(vec)), block_size_(ids_.size())
    {}

    size_type size() const { return ids_.size(); }
    size_type block_size() const { return block_size_; }
    void block_size(size_type block_size) { block_size_ = block_size; }

    int num_blocks() const
    { return (ids_.size() + block_size_ - 1) / block_size_; }

    const value_type& operator[](std::size_t pos) const { return ids_[pos]; }

    iterator begin() const { return iterator(*this, 0, 0); }
    iterator end() const
    {
        auto block = ids_.size() / block_size_;
        auto pos = ids_.size() % block_size_;
        return iterator(*this, block, pos);
    }
    iterator lookup(value_type id) const { return begin().nextgeq(id); }

private:
    std::vector<value_type> ids_;
    size_type block_size_ = 0;
};

template<class PayloadT>
class vector_payload_list {
public:
    using size_type = long;
    using value_type = PayloadT;
    using self_type = vector_payload_list<PayloadT>;
    using iterator = vector_block_iterator<self_type, false>;
    using const_iterator = iterator;

    vector_payload_list(std::vector<value_type> vec)
        : ids_(std::move(vec)), block_size_(ids_.size())
    {}

    size_type size() const { return ids_.size(); }
    size_type block_size() const { return block_size_; }
    void block_size(size_type block_size) { block_size_ = block_size; }

    int num_blocks() const
    { return (ids_.size() + block_size_ - 1) / block_size_; }

    const value_type& operator[](std::size_t pos) const { return ids_[pos]; }

    iterator begin() const { return iterator(*this, 0, 0); }
    iterator end() const
    {
        auto block = ids_.size() / block_size_;
        auto pos = ids_.size() % block_size_;
        return iterator(*this, block, pos);
    }

    template<class DocumentIterator>
    iterator at(DocumentIterator pos) const
    { return begin().aligned(pos); }

private:
    std::vector<value_type> ids_;
    size_type block_size_ = 0;
};

};  // namespace irk
