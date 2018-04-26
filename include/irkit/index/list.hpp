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

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/stream.hpp>
#include <gsl/span>
#include <irkit/coding.hpp>
#include <irkit/index/block.hpp>
#include <irkit/index/skiplist.hpp>
#include <irkit/memoryview.hpp>
#include <vector>

namespace irk::index
{

//! Document iterator
template<class ListView, bool delta_encoded>
class block_iterator : public boost::iterator_facade<
        block_iterator<ListView, delta_encoded>,
        const typename ListView::value_type,
        boost::forward_traversal_tag> {
public:
    using view_type = ListView;
    using self_type = block_iterator<view_type, delta_encoded>;
    using codec_type = typename view_type::codec_type;
    using difference_type = long;
    using value_type = typename ListView::value_type;

    block_iterator() = default;
    block_iterator(const self_type&) = default;
    block_iterator(
        const view_type& view, difference_type block, difference_type pos)
        : view_(view),
          block_(block),
          pos_(pos),
          decoded_blocks_(view_.blocks_.size(), std::nullopt)
    {}

    template<class = std::enable_if_t<delta_encoded>>
    self_type& next_ge(value_type val)
    {
        int block = next_ge_block(block_, val);
        if (block >= decoded_blocks_.size())
        {
            finish();
            return *this;
        }
        pos_ = block == block_ ? pos_ : 0;
        block_ = block;
        ensure_decoded();
        while ((*decoded_blocks_[block_])[pos_] < val) { pos_++; }
        return *this;
    };

    template<class BlockIterator>
    self_type& align(const BlockIterator& other)
    {
        block_ = other.block();
        pos_ = other.pos();
    }

    long block() const { return block_; }
    long pos() const { return pos_; }

private:
    friend class boost::iterator_core_access;
    void increment()
    {
        block_ += (pos_ + 1) / view_.block_size_;
        pos_ = (pos_ + 1) % view_.block_size_;
    }
    void advance(difference_type n)
    {
        block_ += (pos_ + n) / view_.block_size_;
        pos_ = (pos_ + n) % view_.block_size_;
    }
    bool equal(const self_type& other) const
    {
        return pos_ == other.pos_ && block_ == other.block_;
    }
    const value_type& dereference() const
    {
        ensure_decoded();
        return (*decoded_blocks_[block_])[pos_];
    }

    void ensure_decoded() const
    {
        if (!decoded_blocks_[block_].has_value())
        {
            if constexpr (delta_encoded)
            {
                auto preceding = block_ > 0
                    ? view_.blocks_[block_ - 1].back() : 0;
                decoded_blocks_[block_] = std::make_optional(
                    irk::coding::decode_delta(
                        view_.blocks_[block_].data(), view_.codec_, preceding));
            } else
            {
                decoded_blocks_[block_] = std::make_optional(
                    irk::coding::decode(
                        view_.blocks_[block_].data(), view_.codec_));
            }
        }
    }

    int next_ge_block(int block, value_type id) const
    {
        while (block < view_.blocks_.size()
            && view_.blocks_[block].back() < id)
        { block++; }
        return block;
    }

    void finish()
    {
        block_ = view_.length_ / view_.block_size_;
        pos_ = view_.length_ % view_.block_size_;
    }

    const view_type& view_;
    std::size_t block_;
    std::size_t pos_;
    mutable std::vector<std::optional<std::vector<value_type>>>
        decoded_blocks_;
};

template<class Codec>
class block_document_list_view {
public:
    using size_type = int;
    using codec_type = Codec;
    using self_type = block_document_list_view<codec_type>;
    using value_type = typename codec_type::value_type;
    using iterator = block_iterator<self_type, true>;

    block_document_list_view(irk::memory_view mem, long length, int offset = 0)
        : length_(length)
    {
        using slice = memory_view::slice_type;

        // TODO: make that more efficient, a dedicated istream
        //       avoid using `data()`.
        const char* list_ptr = mem.data() + offset;
        boost::iostreams::stream_buffer<
            boost::iostreams::basic_array_source<char>>
            buf(list_ptr, mem.size() - offset);
        std::istream istr(&buf);

        irk::coding::varbyte_codec<long> vb;
        long list_byte_size, num_blocks;
        vb.decode(istr, list_byte_size);
        vb.decode(istr, block_size_);
        vb.decode(istr, num_blocks);

        std::vector<long> skips = irk::coding::decode_n(istr, num_blocks, vb);
        std::vector<value_type> last_documents = irk::coding::decode_delta_n(
            istr, num_blocks, codec_);

        int running_offset = offset + istr.tellg();

        for (int block = 0; block < num_blocks - 1; block++) {
            running_offset += skips[block];
            blocks_.emplace_back(last_documents[block],
                mem.range(running_offset, skips[block + 1]));
        }
        running_offset += skips.back();
        blocks_.emplace_back(last_documents.back(),
            mem[slice(running_offset, offset + list_byte_size - 1)]);
    }

    iterator begin() const { return iterator{*this, 0, 0}; };
    iterator end() const
    {
        return iterator{*this, length_ / block_size_, length_ % block_size_};
    };
    iterator lookup(value_type id) const { return begin().next_ge(id); };

private:
    friend class block_iterator<self_type, true>;
    long length_;
    std::vector<irk::index::block_view<value_type>> blocks_;
    codec_type codec_;
    long block_size_;
};

template<class Codec>
class block_payload_list_view {
public:
    using size_type = int;
    using codec_type = Codec;
    using self_type = block_payload_list_view<codec_type>;
    using value_type = typename codec_type::value_type;
    using iterator = block_iterator<self_type, false>;

    block_payload_list_view(irk::memory_view mem, long length, int offset = 0)
        : length_(length)
    {
        using slice = memory_view::slice_type;

        // TODO: make that more efficient, a dedicated istream
        //       avoid using `data()`.
        const char* list_ptr = mem.data() + offset;
        boost::iostreams::stream_buffer<
            boost::iostreams::basic_array_source<char>>
            buf(list_ptr, mem.size() - offset);
        std::istream istr(&buf);

        irk::coding::varbyte_codec<long> vb;
        long list_byte_size, num_blocks;
        vb.decode(istr, list_byte_size);
        vb.decode(istr, block_size_);
        vb.decode(istr, num_blocks);

        std::vector<long> skips = irk::coding::decode_n(istr, num_blocks, vb);

        int running_offset = offset + istr.tellg();

        for (int block = 0; block < num_blocks - 1; block++) {
            running_offset += skips[block];
            blocks_.emplace_back(mem.range(running_offset, skips[block + 1]));
        }
        running_offset += skips.back();
        blocks_.emplace_back(
            mem[slice(running_offset, offset + list_byte_size - 1)]);
    }

    iterator begin() const { return iterator{*this, 0, 0}; };
    iterator end() const
    {
        return iterator{*this, length_ / block_size_, length_ % block_size_};
    };
    iterator lookup(value_type id) const { return begin().next_ge(id); };

private:
    friend class block_iterator<self_type, false>;
    long length_;
    std::vector<irk::index::block_view<>> blocks_;
    codec_type codec_;
    long block_size_;
};

template<class Codec>
class block_posting_list_view {
public:
private:
    block_document_list_view document_list_;
    std::vector<block_document_list_view document_list_;
};

};  // namespace irk::index
