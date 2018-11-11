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
#include <irkit/sgn.hpp>

namespace irk::index
{

using irk::literals::operator""_id;

struct block_position_t {
    int32_t block;
    int32_t off;
    block_position_t() = default;
    block_position_t(int32_t block, int32_t off) : block(block), off(off) {}
    block_position_t(const block_position_t& other) = default;
    block_position_t(block_position_t&& other) = default;
    block_position_t& operator=(const block_position_t& other) = default;
    block_position_t& operator=(block_position_t&& other) = default;
    ~block_position_t() = default;

    bool operator==(const block_position_t& other) const
    {
        const int64_t* p1 = reinterpret_cast<const int64_t*>(&block);
        const int64_t* p2 = reinterpret_cast<const int64_t*>(&other.block);
        return *p1 == *p2;
    }

    bool operator!=(const block_position_t& other) const
    {
        auto p1 = reinterpret_cast<const int64_t*>(&block);
        auto p2 = reinterpret_cast<const int64_t*>(&other.block);
        return *p1 != *p2;
    }
};

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

    block_iterator(const block_iterator&) = default;
    block_iterator(block_iterator&&) noexcept = default;
    block_iterator& operator=(const block_iterator&) = default;
    block_iterator& operator=(block_iterator&&) noexcept = default;
    ~block_iterator() = default;

    /*!
     * The iterator is strictly connected to a list view and it stores a const
     * reference to it internally. However, for performance reasons, when
     * comparing iterators, it is not checked whether they belong to the same
     * list: this check must be performed by the user if required.
     */
    block_iterator(
        const view_type& view,
        difference_type block,
        difference_type pos,
        int block_size)
        : view_(std::cref(view)),
          pos_(block, pos),
          block_size_(block_size),
          block_count_(view_.get().blocks_.size())
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
        int block = nextgeq_block(pos_.block, val);
        if (block >= block_count_)
        {
            finish();
            return *this;
        }
        pos_.off = block == pos_.block ? pos_.off : 0;
        pos_.block = block;
        ensure_decoded();
        const auto& decoded_block = view_.get().decoded_blocks_[block];
        while (decoded_block[pos_.off] < val) { pos_.off++; }
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
        pos_.block = other.block();
        pos_.off = other.pos();
        return *this;
    }

    //! Returns the current block number.
    int block() const { return pos_.block; }

    //! Returns the current position within the current block.
    int pos() const { return pos_.off; }

    //! Returns the index of the current posting.
    int idx() const { return block_size_ * pos_.block + pos_.off; }

    //! Returns block size.
    auto block_size() const { return block_size_; }

    const index::term_id_t& term_id() const { return view_.get().term_id(); }

private:
    friend class boost::iterator_core_access;
    void increment()
    {
        pos_.off++;
        pos_.block += pos_.off / view_.get().block_size_;
        pos_.off %= view_.get().block_size_;
    }
    void advance(difference_type n)
    {
        pos_.off += n;
        pos_.block += pos_.off / view_.get().block_size_;
        pos_.off %= view_.get().block_size_;
    }
    bool equal(const self_type& other) const { return pos_ == other.pos_; }
    const value_type& dereference() const
    {
        ensure_decoded();
        return view_.get().decoded_blocks_[pos_.block][pos_.off];
    }

    //! Decodes and caches the current block if not decoded.
    void ensure_decoded() const
    {
        auto& decoded_block = view_.get().decoded_blocks_[pos_.block];
        if (decoded_block.empty())
        {
            auto count = pos_.block < block_count_ - 1
                ? block_size_
                : view_.get().length_ - ((block_count_ - 1) * block_size_);
            if constexpr (delta_encoded) {  // NOLINT
                auto preceding = pos_.block > 0
                    ? view_.get().blocks_[pos_.block - 1].back()
                    : 0_id;
                decoded_block.resize(block_size_);
                view_.get().codec_.delta_decode(
                    std::begin(view_.get().blocks_[pos_.block].data()),
                    std::begin(decoded_block),
                    count,
                    preceding);
            }
            else {
                decoded_block.resize(block_size_);
                view_.get().codec_.decode(
                    std::begin(view_.get().blocks_[pos_.block].data()),
                    std::begin(decoded_block),
                    count);
            }
        }
    }

    //! Returns the block of the next greater of equal element.
    int nextgeq_block(int block, value_type id) const
    {
        while (block < sgn(view_.get().blocks_.size())
               && view_.get().blocks_[block].back() < id) {
            block++;
        }
        return block;
    }

    //! Emulates `end()` call on the view.
    void finish()
    {
        pos_.block = view_.get().length_ / view_.get().block_size_;
        pos_.off = (view_.get().length_ - ((block_count_ - 1) * block_size_))
            % block_size_;
    }

    std::reference_wrapper<const view_type> view_;
    block_position_t pos_;
    int32_t block_size_;
    int32_t block_count_;
};

template<class Value, class Codec, bool delta_encoded = false>
class block_list_builder {
public:
    using codec_type = Codec;
    using value_type = Value;
    using encoding_device =
        boost::iostreams::back_insert_device<std::vector<char>>;

    explicit block_list_builder(int block_size) : block_size_(block_size) {}
    block_list_builder(const block_list_builder&) = default;
    block_list_builder(block_list_builder&&) noexcept = default;
    block_list_builder& operator=(const block_list_builder&) = default;
    block_list_builder& operator=(block_list_builder&&) noexcept = default;
    virtual ~block_list_builder() = default;

    void add(value_type id) { values_.push_back(id); }

    std::streamsize write(std::ostream& out) const
    {
        std::vector<int32_t> absolute_skips;
        std::vector<value_type> last_values;
        std::vector<char> encoded_blocks;

        gsl::index pos = 0;
        value_type previous_doc(0);
        (void)previous_doc;  // To remove -Wunused-but-set-variable warning due
                             // to `if constexpr` block later on.
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

    auto size() const { return values_.size(); }
    const auto& values() const { return values_; }

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

template<class T, class Codec, class L = std::nullopt_t>
class block_list_view {
    bool constexpr static delta_encoded = not std::is_same_v<L, std::nullopt_t>;

public:
    using size_type = int32_t;
    using value_type = T;
    using self_type = block_list_view;
    using iterator = block_iterator<self_type, delta_encoded>;
    using codec_type = Codec;

    block_list_view() = default;
    block_list_view(term_id_t term_id, irk::memory_view mem, int32_t length)
        : term_id_(term_id), length_(length), memory_(std::move(mem))
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
        decoded_blocks_.resize(num_blocks);

        std::vector<int64_t> skips(num_blocks);
        pos = vb.decode(pos, &skips[0], num_blocks);

        if constexpr (delta_encoded) {
            std::vector<value_type> last_documents(num_blocks);
            pos = codec_.delta_decode(pos, &last_documents[0], num_blocks);

            blocks_.reserve(num_blocks);
            for (int block = 0; block < num_blocks - 1; block++) {
                std::advance(pos, skips[block]);
                blocks_.emplace_back(last_documents[block],
                    irk::make_memory_view(pos, skips[block + 1]));
            }
            std::advance(pos, skips.back());
            blocks_.emplace_back(last_documents.back(),
                irk::make_memory_view(
                    pos, std::distance(pos, std::end(memory_))));
        }
        else {
            blocks_.reserve(num_blocks);
            for (int block = 0; block < num_blocks - 1; block++) {
                std::advance(pos, skips[block]);
                blocks_.emplace_back(
                    irk::make_memory_view(pos, skips[block + 1]));
            }
            std::advance(pos, skips.back());
            blocks_.emplace_back(irk::make_memory_view(
                pos, std::distance(pos, std::end(memory_))));
        }
    }

    iterator begin() const { return iterator{*this, 0, 0, block_size_}; };
    iterator end() const
    {
        if (length_ == 0) { return begin(); };
        auto end_pos =
            (length_ - ((static_cast<int>(blocks_.size()) - 1) * block_size_))
            % block_size_;
        return iterator{*this, length_ / block_size_, end_pos, block_size_};
    };

    //! Finds the position of `id` or the next greater.
    iterator lookup(value_type id) const { return begin().nextgeq(id); };

    int32_t size() const { return length_; }
    int32_t block_size() const { return block_size_; }
    int64_t memory_size() const { return memory_.size(); }
    memory_view memory() const { return memory_; }

    std::ostream& write(std::ostream& out) const
    {
        return out.write(memory_.data(), memory_.size());
    }

    const index::term_id_t& term_id() const { return term_id_; }

private:
    index::term_id_t term_id_{};
    int32_t length_ = 0;
    int32_t block_size_ = 0;
    irk::memory_view memory_{};
    codec_type codec_{};
    std::vector<irk::index::block_view<L>> blocks_{};
    mutable std::vector<std::vector<value_type>> decoded_blocks_{};

    friend class block_iterator<self_type, delta_encoded>;
};

template<class Codec>
using block_document_list_view = block_list_view<document_t, Codec, document_t>;

template<class Payload, class Codec>
using block_payload_list_view = block_list_view<Payload, Codec, std::nullopt_t>;

}  // namespace irk::index
