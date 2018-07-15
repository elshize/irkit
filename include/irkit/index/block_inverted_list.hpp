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

namespace irk::index
{

using irk::literals::operator""_id;

//! Block iterator.
/*!
 * \tparam ListView         type of list (see \ref block_document_list_view and
 *                          \ref block_payload_list_view)
 * \tparam delta_encoded    whether or not values are delta encoded; if so, an
 *                          additional operation `nextgeq(value_type)` is
 *                          available to skip to the next greater or equal
 *                          value
 *
 * This iterator will decode and cache blocks that are accessed. The following
 * operation will cause decoding of the entire block:
 *  - dereferencing a value in a not yet decoded block;
 *  - `nextgeq` operation that reaches a not yet decoded block.
 *
 * Any other operations will not decode or access any data.
 */
template<class ListView, bool delta_encoded>
class block_iterator : public boost::iterator_facade<
        block_iterator<ListView, delta_encoded>,
        const typename ListView::value_type,
        boost::forward_traversal_tag> {
public:
    using view_type = ListView;
    using self_type = block_iterator<view_type, delta_encoded>;
    using difference_type = int;
    using value_type = typename ListView::value_type;

    block_iterator(const self_type&) = default;
    block_iterator(self_type&&) noexcept = default;
    block_iterator& operator=(const self_type&) = delete;
    block_iterator& operator=(self_type&&) noexcept = delete;
    ~block_iterator() = default;

    /*!
     * The iterator is strictly connected to a list view and it stores a const
     * reference to it internally. However, for performance reasons, when
     * comparing iterators, it is not checked whether they belong to the same
     * list: this check must be performed by the user if required.
     */
    block_iterator(const view_type& view,
        difference_type block,
        difference_type pos,
        int block_size)
        : view_(view),
          block_(block),
          pos_(pos),
          block_size_(block_size),
          block_count_(view_.blocks_.size()),
          decoded_block_num_(-1),
          decoded_block_(block_size_)
    {}

    //! Move to the next position greater or equal `val`.
    /*!
     * \param val   the value to look up; if does not exist, point to the next
     *              greater value; if neither exist, point to `end()`
     * \returns     `*this` after finding position
     *
     * The lookup starts from the current position. If the next value `>= val`
     * exists in a still encoded block, it will be decoded and cached.
     * If all values from the current position forward are `< val`, then
     * the resulting iterator will point after the last element (equivalent to
     * calling `end()` on the list view).
     */
    self_type& moveto(value_type val)
    {
        static_assert(delta_encoded, "must be delta encoded to call moveto");
        int block = nextgeq_block(block_, val);
        if (block >= block_count_)
        {
            finish();
            return *this;
        }
        pos_ = block == block_ ? pos_ : 0;
        block_ = block;
        ensure_decoded();
        while (decoded_block_[pos_] < val) { pos_++; }
        return *this;
    };

    self_type nextgeq(value_type val)
    {
        static_assert(delta_encoded, "must be delta encoded to call nextgeq");
        self_type next(*this);
        next.moveto(val);
        return next;
    };

    //! Align this iterator to another.
    /*!
     * Sets the position of this iterator to the position of `other`.
     * Both lists should have the same length and block size, but this
     * is not checked at runtime!
     */
    template<class BlockIterator>
    self_type& align(const BlockIterator& other)
    {
        block_ = other.block();
        pos_ = other.pos();
        return *this;
    }

    //! Returns the current block number.
    int block() const { return block_; }

    //! Returns the current position within the current block.
    int pos() const { return pos_; }

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
        return decoded_block_[pos_];
    }

    //! Decodes and caches the current block if not decoded.
    void ensure_decoded() const
    {
        if (block_ != decoded_block_num_)
        {
            auto count = block_ < block_count_ - 1
                ? block_size_
                : view_.length_ - ((block_count_ - 1) * block_size_);
            if constexpr (delta_encoded) {  // NOLINT
                auto preceding = block_ > 0 ? view_.blocks_[block_ - 1].back()
                                            : 0_id;
                view_.codec_.delta_decode(
                    std::begin(view_.blocks_[block_].data()),
                    std::begin(decoded_block_),
                    count,
                    preceding);
            }
            else {
                decoded_block_.resize(block_size_);
                view_.codec_.decode(std::begin(view_.blocks_[block_].data()),
                    std::begin(decoded_block_),
                    count);
            }
            decoded_block_num_ = block_;
        }
    }

    //! Returns the block of the next greater of equal element.
    int nextgeq_block(int block, value_type id) const
    {
        while (block < view_.blocks_.size()
            && view_.blocks_[block].back() < id)
        { block++; }
        return block;
    }

    //! Emulates `end()` call on the view.
    void finish()
    {
        block_ = view_.length_ / view_.block_size_;
        pos_ = view_.length_ % view_.block_size_;
    }

    int block_size_;
    const view_type& view_;
    int block_;
    int pos_;
    const std::size_t block_count_;
    mutable int decoded_block_num_;
    mutable std::vector<value_type> decoded_block_;
};

template<class Value, class Codec, bool delta_encoded = false>
class block_list_builder {
public:
    using codec_type = Codec;
    using value_type = Value;
    using encoding_device =
        boost::iostreams::back_insert_device<std::vector<char>>;

    explicit block_list_builder(int block_size) : block_size_(block_size) {}

    void add(value_type id) { values_.push_back(id); }

    std::streamsize write(std::ostream& out) const
    {
        std::vector<int32_t> absolute_skips;
        std::vector<value_type> last_values;
        std::vector<char> encoded_blocks;

        gsl::index pos = 0;
        value_type previous_doc(0);
        const int32_t num_blocks = (values_.size() + block_size_ - 1)
            / block_size_;
        for (gsl::index block = 0; block < num_blocks; ++block)
        {
            absolute_skips.push_back(pos);
            const gsl::index begin_idx = block_size_ * block;
            const gsl::index end_idx = std::min(
                begin_idx + block_size_, gsl::index(values_.size()));

            const value_type* begin = values_.data() + begin_idx;
            const value_type* end = values_.data() + end_idx;

            encoded_blocks.resize(
                pos + value_codec_.max_encoded_size(end_idx - begin_idx));
            if constexpr (delta_encoded) {  // NOLINT
                last_values.push_back(values_[end_idx - 1]);
                pos += value_codec_.delta_encode(
                    begin, end, &encoded_blocks[pos], previous_doc);
                previous_doc = last_values.back();
            }
            else {
                pos += value_codec_.encode(begin, end, &encoded_blocks[pos]);
            }
        }

        const std::vector<char> encoded_header = irk::encode(
            int_codec_, {block_size_, num_blocks});
        const std::vector<char> encoded_skips = irk::delta_encode(
            int_codec_, absolute_skips);

        int list_byte_size = encoded_header.size() + encoded_skips.size() + pos;
        std::vector<char> encoded_last_vals;

        if constexpr (delta_encoded) {  // NOLINT
            encoded_last_vals = irk::delta_encode(
                value_codec_, last_values);  // NOLINT
            list_byte_size += encoded_last_vals.size();
        }  // NOLINT
        list_byte_size = expanded_size(list_byte_size);  // NOLINT

        auto encoded_list_byte_size = irk::encode(int_codec_, {list_byte_size});
        out.write(&encoded_list_byte_size[0], encoded_list_byte_size.size());
        out.write(&encoded_header[0], encoded_header.size());
        out.write(&encoded_skips[0], encoded_skips.size());
        if constexpr (delta_encoded) {  // NOLINT
            out.write(
                &encoded_last_vals[0], encoded_last_vals.size());  // NOLINT
        }
        out.write(&encoded_blocks[0], pos);  // NOLINT

        return list_byte_size;
    }

private:
    int expanded_size(int list_byte_size) const
    {
        unsigned int extra_bytes = 1;
        while (static_cast<unsigned int>(list_byte_size) + extra_bytes
            >= (1u << (extra_bytes * 7u)))
        { extra_bytes++; }
        return static_cast<int>(list_byte_size + extra_bytes);
    }

    int32_t block_size_;
    codec_type value_codec_;
    std::vector<value_type> values_;
    irk::vbyte_codec<int32_t> int_codec_;
};

//! A view of a block document list.
/*!
 * \tparam Doc  type of document ID, must be integer.
 */
template<class Codec>
class block_document_list_view {
public:
    using size_type = int32_t;
    using value_type = document_t;
    using self_type = block_document_list_view;
    using iterator = block_iterator<self_type, true>;
    using codec_type = Codec;

    //! Constructs the view.
    /*!
     * \param doc_codec     a codec for document IDs
     * \param mem           a memory view containing the list
     * \param length        the number of elements in the list
     * \param offset        offset from the beginning of `mem`
     */
    block_document_list_view(irk::memory_view mem, int32_t length)
        : length_(length), memory_(std::move(mem))
    {
        auto pos = memory_.begin();
        irk::vbyte_codec<int64_t> vb;

        int64_t list_byte_size, num_blocks;
        pos = vb.decode(pos, &list_byte_size);
        pos = vb.decode(pos, &block_size_);
        pos = vb.decode(pos, &num_blocks);
        if (list_byte_size != memory_.size()) {
            std::ostringstream str;
            str << "list size " << list_byte_size
                << " does not match memory view size " << memory_.size();
            throw std::runtime_error(str.str());
        }

        std::vector<int64_t> skips(num_blocks);
        pos = vb.decode(pos, &skips[0], num_blocks);
        std::vector<value_type> last_documents(num_blocks);
        pos = codec_.delta_decode(pos, &last_documents[0], num_blocks);

        for (int block = 0; block < num_blocks - 1; block++) {
            std::advance(pos, skips[block]);
            blocks_.emplace_back(last_documents[block],
                irk::make_memory_view(pos, skips[block + 1]));
        }
        std::advance(pos, skips.back());
        blocks_.emplace_back(last_documents.back(),
            irk::make_memory_view(pos, std::distance(pos, std::end(memory_))));
    }

    iterator begin() const { return iterator{*this, 0, 0, block_size_}; };
    iterator end() const
    {
        return iterator{
            *this, length_ / block_size_, length_ % block_size_, block_size_};
    };

    //! Finds the position of `id` or the next greater.
    iterator lookup(value_type id) const { return begin().nextgeq(id); };

    int32_t size() const { return length_; }
    int64_t memory_size() const { return memory_.size(); }
    memory_view memory() const { return memory_; }

    std::ostream& write(std::ostream& out) const
    {
        return out.write(memory_.data(), memory_.size());
    }

private:
    friend class block_iterator<self_type, true>;
    int32_t length_ = 0;
    codec_type codec_;
    std::vector<irk::index::block_view<value_type>> blocks_;
    int32_t block_size_ = 0;
    irk::memory_view memory_;
};

//! A view of a block payload list.
/*!
 * \tparam Payload  type of the payload, must be primitive type
 */
template<class Payload, class Codec>
class block_payload_list_view {
public:
    using size_type = int;
    using value_type = Payload;
    using codec_type = Codec;
    using self_type = block_payload_list_view<value_type, codec_type>;
    using iterator = block_iterator<self_type, false>;

    block_payload_list_view() = default;

    //! Constructs the view.
    /*!
     * \param doc_codec     a codec for document IDs
     * \param mem           a memory view containing the list
     * \param length        the number of elements in the list
     * \param offset        offset from the beginning of `mem`
     */
    block_payload_list_view(irk::memory_view mem, int32_t length)
        : length_(length), memory_(std::move(mem))
    {
        auto pos = memory_.begin();
        irk::vbyte_codec<int64_t> vb;

        int64_t list_byte_size, num_blocks;
        pos = vb.decode(pos, &list_byte_size);
        pos = vb.decode(pos, &block_size_);
        pos = vb.decode(pos, &num_blocks);
        if (list_byte_size != memory_.size()) {
            std::ostringstream str;
            str << "list size " << list_byte_size
                << " does not match memory view size " << memory_.size();
            throw std::runtime_error(str.str());
        }

        std::vector<int64_t> skips(num_blocks);
        pos = vb.decode(pos, &skips[0], num_blocks);

        for (int block = 0; block < num_blocks - 1; block++)
        {
            std::advance(pos, skips[block]);
            blocks_.emplace_back(irk::make_memory_view(pos, skips[block + 1]));
        }
        std::advance(pos, skips.back());
        blocks_.emplace_back(
            irk::make_memory_view(pos, std::distance(pos, std::end(memory_))));
    }

    iterator begin() const { return iterator{*this, 0, 0, block_size_}; };
    iterator end() const
    {
        return iterator{
            *this, length_ / block_size_, length_ % block_size_, block_size_};
    };
    int32_t size() const { return length_; }
    int64_t memory_size() const { return memory_.size(); }

    std::ostream write(std::ostream& out) const
    {
        return out.write(memory_.data(), memory_.size());
    }

private:
    friend class block_iterator<self_type, false>;
    int32_t length_ = 0;
    codec_type codec_;
    std::vector<irk::index::block_view<>> blocks_;
    int32_t block_size_ = 0;
    irk::memory_view memory_;
};

};  // namespace irk::index
