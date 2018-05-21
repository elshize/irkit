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

#include <vector>

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/type_erasure/any.hpp>
#include <boost/type_erasure/builtin.hpp>
#include <boost/type_erasure/concept_interface.hpp>
#include <boost/type_erasure/rebind_any.hpp>
#include <gsl/span>

#include <irkit/coding.hpp>
#include <irkit/coding/copy.hpp>
#include <irkit/index/block.hpp>
#include <irkit/index/skiplist.hpp>
#include <irkit/memoryview.hpp>

namespace irk::index
{

//! Block iterator.
/*!
 * \tparam ListView         type of list (see \ref block_document_list_view and
 *                          \ref block_payload_list_view)
 * \tparam delta_encoded    whether or not values are delta encoded; if so, an
 *                          additional operation `next_ge(value_type)` is
 *                          available to skip to the next greater or equal
 *                          value
 *
 * This iterator will decode and cache blocks that are accessed. The following
 * operation will cause decoding of the entire block:
 *  - dereferencing a value in a not yet decoded block;
 *  - `next_ge` operation that reaches a not yet decoded block.
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
    using difference_type = long;
    using value_type = typename ListView::value_type;

    block_iterator() = default;
    block_iterator(const self_type&) = default;

    /*!
     * The iterator is strictly connected to a list view and it stores a const
     * reference to it internally. However, for performance reasons, when
     * comparing iterators, it is not checked whether they belong to the same
     * list: this check must be performed by the user if required.
     */
    block_iterator(
        const view_type& view, difference_type block, difference_type pos)
        : view_(view),
          block_(block),
          pos_(pos),
          decoded_blocks_(view_.blocks_.size(), std::nullopt)
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
    }

    //! Returns the current block number.
    long block() const { return block_; }

    //! Returns the current position within the current block.
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

    //! Decodes and caches the current block if not decoded.
    void ensure_decoded() const
    {
        if (!decoded_blocks_[block_].has_value())
        {
            decoded_blocks_[block_] = std::make_optional(
                std::vector<value_type>(view_.block_size_));
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

    //! Returns the block of the next greater of equal element.
    int next_ge_block(int block, value_type id) const
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

    const view_type& view_;
    std::size_t block_;
    std::size_t pos_;
    mutable std::vector<std::optional<std::vector<value_type>>>
        decoded_blocks_;
};

template<class Value, bool delta_encoded = false>
class block_list_builder {
public:
    using value_type = Value;
    using encoding_device =
        boost::iostreams::back_insert_device<std::vector<char>>;

    block_list_builder(int block_size, irk::any_codec<value_type> value_codec)
        : block_size_(block_size), value_codec_(value_codec)
    {}

    void add(value_type id) { values_.push_back(id); }

    std::streamsize write(std::ostream& out) const
    {
        std::vector<int> absolute_skips;
        std::vector<value_type> last_values;
        std::vector<char> encoded_blocks;
        boost::iostreams::stream<encoding_device> val_stream(
            boost::iostreams::back_inserter(encoded_blocks));

        value_type previous_doc(0);
        const int num_blocks = (values_.size() + block_size_ - 1) / block_size_;
        for (gsl::index block = 0; block < num_blocks; ++block)
        {
            absolute_skips.push_back(encoded_blocks.size());
            const gsl::index begin_idx = block_size_ * block;
            const gsl::index end_idx = std::min(
                begin_idx + block_size_, gsl::index(values_.size()));

            if constexpr (delta_encoded)
            { last_values.push_back(values_[end_idx - 1]); }

            for (gsl::index idx = begin_idx; idx < end_idx; ++idx)
            {
                if constexpr (delta_encoded) {
                    const auto delta = values_[idx] - previous_doc;
                    value_codec_.encode(delta, val_stream);
                    previous_doc = values_[idx];
                } else {
                    value_codec_.encode(values_[idx], val_stream);
                }
            }
            val_stream.flush();
        }

        const std::vector<char> encoded_header = irk::coding::encode(
            {block_size_, num_blocks}, int_codec_);
        const std::vector<char> encoded_skips = irk::coding::encode_delta(
            absolute_skips, int_codec_);

        int list_byte_size = encoded_header.size() + encoded_skips.size()
            + encoded_blocks.size();
        std::vector<char> encoded_last_vals;

        if constexpr (delta_encoded)
        {
            encoded_last_vals = irk::coding::encode_delta(
                last_values, value_codec_);
            list_byte_size += encoded_last_vals.size();
        }
        list_byte_size = expanded_size(list_byte_size);

        int_codec_.encode(list_byte_size, out);
        out.write(&encoded_header[0], encoded_header.size());
        out.write(&encoded_skips[0], encoded_skips.size());
        if constexpr (delta_encoded)
        { out.write(&encoded_last_vals[0], encoded_last_vals.size()); }
        out.write(&encoded_blocks[0], encoded_blocks.size());
        out.flush();

        return list_byte_size;
    }

private:
    int expanded_size(int list_byte_size) const
    {
        int extra_bytes = 1;
        while (list_byte_size + extra_bytes >= (1 << (extra_bytes * 7)))
        { extra_bytes++; }
        return list_byte_size + extra_bytes;
    }

    int block_size_;
    any_codec<value_type> value_codec_;
    std::vector<value_type> values_;
    irk::coding::varbyte_codec<int> int_codec_;
};

//! A view of a block document list.
/*!
 * \tparam Doc  type of document ID, must be integer.
 */
template<class Doc>
class block_document_list_view {
public:
    using size_type = long;
    using value_type = Doc;
    using self_type = block_document_list_view<value_type>;
    using iterator = block_iterator<self_type, true>;

    //! Constructs the view.
    /*!
     * \param doc_codec     a codec for document IDs
     * \param mem           a memory view containing the list
     * \param length        the number of elements in the list
     * \param offset        offset from the beginning of `mem`
     */
    block_document_list_view(any_codec<value_type> doc_codec,
        irk::memory_view mem,
        long length,
        int offset = 0)
        : length_(length), codec_(doc_codec)
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

    //! Finds the position of `id` or the next greater.
    iterator lookup(value_type id) const { return begin().next_ge(id); };

private:
    friend class block_iterator<self_type, true>;
    long length_;
    any_codec<Doc> codec_;
    std::vector<irk::index::block_view<value_type>> blocks_;
    long block_size_;
};

//! A view of a block payload list.
/*!
 * \tparam Payload  type of the payload, must be primitive type
 */
template<class Payload>
class block_payload_list_view {
public:
    using size_type = int;
    using value_type = Payload;
    using self_type = block_payload_list_view<value_type>;
    using iterator = block_iterator<self_type, false>;

    block_payload_list_view()
        : length_(0), codec_(std::move(irk::coding::copy_codec<value_type>{}))
    {}

    //! Constructs the view.
    /*!
     * \param doc_codec     a codec for document IDs
     * \param mem           a memory view containing the list
     * \param length        the number of elements in the list
     * \param offset        offset from the beginning of `mem`
     */
    block_payload_list_view(any_codec<value_type> payload_codec,
        irk::memory_view mem,
        long length,
        int offset = 0)
        : length_(length), codec_(payload_codec)
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

private:
    friend class block_iterator<self_type, false>;
    long length_;
    any_codec<Payload> codec_;
    std::vector<irk::index::block_view<>> blocks_;
    long block_size_;
};

template<class ...Payload>
class zipped_payload_list_view {
public:
    using value_type = std::tuple<Payload...>;
    using reference = value_type;
    using difference_type = long;

    zipped_payload_list_view() = default;
    zipped_payload_list_view(block_payload_list_view<Payload>... views)
        : views_(std::make_tuple(views...))
    {}

    class iterator : public boost::iterator_facade<iterator,
                         value_type,
                         boost::forward_traversal_tag,
                         reference> {
    public:
        iterator(typename block_payload_list_view<Payload>::iterator... iterators)
            : iterators_(iterators...)
        {}

        template<class BlockIterator, std::size_t... I>
        iterator& align(const BlockIterator& other, std::index_sequence<I...>)
        {
            (std::get<I>(iterators_).align(other), ...);
            return *this;
        }

        template<class BlockIterator>
        iterator& align(const BlockIterator& other)
        {
            return align(other, index_sequence_);
        }

        template<std::size_t idx>
        auto payload() const
        {
            return *std::get<idx>(iterators_);
        }

    private:
        friend class boost::iterator_core_access;

        constexpr static std::index_sequence_for<Payload...> index_sequence_ =
            std::index_sequence_for<Payload...>{};

        template<std::size_t... I>
        void increment(std::index_sequence<I...>)
        { (std::get<I>(iterators_)++, ...); }
        void increment() { increment(index_sequence_); }

        template<std::size_t... I>
        void advance(std::index_sequence<I...>, difference_type n)
        {
            ((std::get<I>(iterators_) += n), ...);
        }
        void advance(difference_type n) { advance(index_sequence_, n); }

        bool equal(const iterator& other) const
        {
            return iterators_ == other.iterators_;
        }

        template<std::size_t... I>
        reference dereference(std::index_sequence<I...>) const
        {
            return std::make_tuple(*std::get<I>(iterators_)...);
        }
        reference dereference() const { return dereference(index_sequence_); }

        std::tuple<
            typename block_payload_list_view<Payload>::iterator...> iterators_;
    };

    iterator begin() const
    { return begin(std::index_sequence_for<Payload...>{}); }

    iterator end() const { return end(std::index_sequence_for<Payload...>{}); }

private:
    template<std::size_t... I>
    iterator begin(std::index_sequence<I...>) const
    {
        return iterator(std::get<I>(views_).begin()...);
    }

    template<std::size_t... I>
    iterator end(std::index_sequence<I...>) const
    {
        return iterator(std::get<I>(views_).end()...);
    }

    std::tuple<block_payload_list_view<Payload>...> views_;
};

//! A view of a block posting list (ID, payload).
/*!
 * \tparam Doc      type of the document ID, must be integer
 * \tparam Payload  a list of types of the payload, must be primitive types
 */
template<class Doc, class... Payload>
class block_posting_list_view {
public:
    using document_type = Doc;
    using difference_type = long;
    using document_iterator_t =
        typename block_document_list_view<document_type>::iterator;
    using payload_iterator_t =
        typename zipped_payload_list_view<Payload...>::iterator;

    //! Document iterator
    template<class payload_type>
    class iterator : public boost::iterator_facade<iterator<payload_type>,
                         std::pair<document_type, payload_type>,
                         boost::forward_traversal_tag,
                         std::pair<document_type, payload_type>> {
    public:
        iterator(document_iterator_t document_iterator,
            payload_iterator_t payload_iterator)
            : document_iterator_(document_iterator),
              payload_iterator_(payload_iterator)
        {}

        iterator& next_ge(document_type doc)
        {
            document_iterator_.next_ge(doc);
            payload_iterator_.align(document_iterator_);
        }
        const document_type& document() const { return *document_iterator_; }

        payload_type payload() const
        {
            if constexpr (sizeof...(Payload) == 1) {
                return std::get<0>(*payload_iterator_);
            } else {
                return *payload_iterator_;
            }
        }

    private:
        friend class boost::iterator_core_access;
        void increment()
        {
            document_iterator_++;
            payload_iterator_++;
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

        std::pair<document_type, payload_type> dereference() const
        {
            if constexpr (sizeof...(Payload) == 1) {
                return std::make_pair(
                    *document_iterator_, std::get<0>(*payload_iterator_));
            } else {
                return std::make_pair(*document_iterator_, *payload_iterator_);
            }
        }

        document_iterator_t document_iterator_;
        payload_iterator_t payload_iterator_;
    };

    block_posting_list_view(block_document_list_view<document_type> documents,
        block_payload_list_view<Payload>... payloads)
        : documents_(documents), payloads_(payloads...)
    {}

    template<class = std::enable_if_t<sizeof...(Payload) == 1>>
    iterator<std::tuple_element_t<0, std::tuple<Payload...>>> begin() const
    {
        return iterator<std::tuple_element_t<0, std::tuple<Payload...>>>(
            documents_.begin(), payloads_.begin());
    };

    template<class = std::enable_if_t<sizeof...(Payload) == 1>>
    iterator<std::tuple_element_t<0, std::tuple<Payload...>>> end() const
    {
        return iterator<std::tuple_element_t<0, std::tuple<Payload...>>>(
            documents_.end(), payloads_.end());
    };

    template<class = std::enable_if_t<sizeof...(Payload) == 1>>
    iterator<std::tuple_element_t<0, std::tuple<Payload...>>>
    lookup(document_type doc) const { return begin().next_ge(doc); };

    template<class = std::enable_if_t<sizeof...(Payload) != 1>>
    iterator<std::tuple<Payload...>> begin() const
    {
        return iterator<std::tuple<Payload...>>(
            documents_.begin(), payloads_.begin());
    };

    template<class = std::enable_if_t<sizeof...(Payload) != 1>>
    iterator<std::tuple<Payload...>> end() const
    {
        return iterator<std::tuple<Payload...>>(
            documents_.end(), payloads_.end());
    };

    template<class = std::enable_if_t<sizeof...(Payload) != 1>>
    iterator<std::tuple<Payload...>> lookup(document_type doc) const
    {
        return begin().next_ge(doc);
    };

private:
    block_document_list_view<document_type> documents_;
    zipped_payload_list_view<Payload...> payloads_;
};

};  // namespace irk::index
