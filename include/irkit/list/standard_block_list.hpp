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

#include <fmt/format.h>

#include <irkit/index/types.hpp>
#include <irkit/iterator/block_iterator.hpp>
#include <irkit/memoryview.hpp>

namespace ir {

void panic(std::string_view error)
{
    std::clog << fmt::format("Fatal error: ", error);
    std::abort();
}

template<class Value, class Codec, bool delta_encoded>
class Standard_Block_List {
public:
    using size_type      = std::int32_t;
    using value_type     = Value;
    using iterator       = Block_Iterator<Standard_Block_List<Value, Codec, delta_encoded>>;
    using const_iterator = iterator;
    using codec_type     = Codec;

    constexpr Standard_Block_List() = default;
    Standard_Block_List(irk::index::term_id_t term_id, irk::memory_view mem, size_type length)
        : term_id_(term_id), length_(length), memory_(std::move(mem))
    {
        auto pos = memory_.begin();
        irk::vbyte_codec<int64_t> vb;
        size_type list_byte_size, num_blocks;
        pos = vb.decode(pos, &list_byte_size);
        pos = vb.decode(pos, &block_size_);
        pos = vb.decode(pos, &num_blocks);
        if (list_byte_size != memory_.size()) {
            panic(fmt::format("list size {} does not match memory view size {} for term {}",
                              list_byte_size,
                              memory_.size(),
                              term_id));
        }
        decoded_blocks_.resize(num_blocks);

        std::vector<size_type> skips(num_blocks);
        pos = vb.decode(pos, &skips[0], num_blocks);

        [[maybe_unused]] std::vector<value_type> last_documents;

        if constexpr (delta_encoded) {
            last_documents.resize(num_blocks);
            pos = codec_.delta_decode(pos, &last_documents[0], num_blocks);
            upper_bounds_.reserve(num_blocks);
        }

        blocks_.reserve(num_blocks);
        for (size_type block : iter::range(num_blocks - 1)) {
            std::advance(pos, skips[block]);
            blocks_.push_back(irk::make_memory_view(pos, skips[block + 1]));
            if constexpr (delta_encoded) {
                upper_bounds_.push_back(last_documents[block]);
            }
        }
        std::advance(pos, skips.back());
        blocks_.push_back(irk::make_memory_view(pos, std::distance(pos, std::end(memory_))));
        if constexpr (delta_encoded) {
            upper_bounds_.push_back(last_documents.back());
        }
    }
    constexpr Standard_Block_List(const Standard_Block_List&)     = default;
    constexpr Standard_Block_List(Standard_Block_List&&) noexcept = default;
    constexpr Standard_Block_List& operator=(const Standard_Block_List&) = default;
    constexpr Standard_Block_List& operator=(Standard_Block_List&&) noexcept = default;

    [[nodiscard]] constexpr auto size() const -> size_type { return length_; }
    [[nodiscard]] constexpr auto block_count() const noexcept -> size_type
    {
        return (size() + block_size_ - 1) / block_size_;
    }

    [[nodiscard]] constexpr auto begin() const noexcept -> iterator
    {
        return iterator({0, 0}, *this);
    }
    [[nodiscard]] constexpr auto end() const noexcept -> iterator
    {
        return iterator{iterator::end(length_, block_size_, blocks_.size()), *this};
    }
    [[nodiscard]] constexpr auto lookup(value_type id) const -> iterator
    {
        static_assert(delta_encoded, "must be delta encoded");
        return begin().next_ge(id);
    }
    [[nodiscard]] constexpr auto term_id() const -> auto const& { return term_id_; }

    [[nodiscard]] constexpr auto block_size() const -> size_type { return block_size_; }
    [[nodiscard]] constexpr auto block_size(size_type n) const noexcept -> size_type
    {
        size_type block_count = blocks_.size();
        return n < block_count - 1 ? block_size_ : length_ - ((block_count - 1) * block_size_);
    }

    [[nodiscard]] constexpr auto block(size_type n) const noexcept -> gsl::span<value_type const>
    {
        auto& decoded_block = decoded_blocks_[n];
        if (decoded_block.empty()) {
            if constexpr (delta_encoded) {  // NOLINT
                decode_delta(n, decoded_block, block_size(n));
            } else {
                decode_no_delta(n, decoded_block, block_size(n));
            }
        }
        return gsl::make_span(decoded_blocks_[n]);
    }

    [[nodiscard]] constexpr auto upper_bounds() const { return upper_bounds_; }
    [[nodiscard]] auto memory() const -> irk::memory_view { return memory_; };

    [[nodiscard]] constexpr static bool is_delta_encoded() { return delta_encoded; }

private:
    constexpr void
    decode_no_delta(size_type block, std::vector<value_type>& buffer, size_type count) const
    {
        buffer.resize(block_size_);
        codec_.decode(std::begin(blocks_[block]), std::begin(buffer), count);
    }

    void decode_delta(size_type block, std::vector<value_type>& buffer, int32_t count) const
    {
        auto preceding = block > 0 ? upper_bounds_[block - 1] : 0;
        buffer.resize(count);
        codec_.delta_decode(std::begin(blocks_[block]), std::begin(buffer), count, preceding);
    }

    irk::index::term_id_t term_id_{};
    size_type length_{0};
    size_type block_size_{1};
    irk::memory_view memory_{};
    codec_type codec_{};
    std::vector<irk::memory_view> blocks_{};
    std::vector<value_type> upper_bounds_{};
    mutable std::vector<std::vector<value_type>> decoded_blocks_{};
};

template<class Codec>
using Standard_Block_Document_List = Standard_Block_List<irk::index::document_t, Codec, true>;

template<class Payload, class Codec>
using Standard_Block_Payload_List = Standard_Block_List<Payload, Codec, false>;

template<class Value, class Codec, bool delta_encoded>
class Standard_Block_List_Builder {
public:
    using size_type = int32_t;
    using codec_type = Codec;
    using value_type = Value;
    using encoding_device = boost::iostreams::back_insert_device<std::vector<char>>;

    explicit constexpr Standard_Block_List_Builder(size_type block_size) : block_size_{block_size}
    {}
    constexpr Standard_Block_List_Builder(const Standard_Block_List_Builder&) = default;
    constexpr Standard_Block_List_Builder(Standard_Block_List_Builder&&) noexcept = default;
    constexpr Standard_Block_List_Builder& operator=(const Standard_Block_List_Builder&) = default;
    constexpr Standard_Block_List_Builder&
    operator=(Standard_Block_List_Builder&&) noexcept = default;
    ~Standard_Block_List_Builder() = default;

    constexpr void add(value_type id) { values_.push_back(id); }

    auto write(std::ostream& out) const -> std::streamsize
    {
        std::vector<int32_t> absolute_skips;
        std::vector<value_type> last_values;
        std::vector<char> encoded_blocks;

        gsl::index pos = 0;
        [[maybe_unused]] value_type previous_doc{0};
        const int32_t num_blocks = (values_.size() + block_size_ - 1) / block_size_;
        for (gsl::index block = 0; block < num_blocks; ++block) {
            absolute_skips.push_back(pos);
            const gsl::index begin_idx = block_size_ * block;
            const gsl::index end_idx = std::min(begin_idx + block_size_,
                                                gsl::index(values_.size()));

            const value_type* begin = values_.data() + begin_idx;
            const value_type* end = values_.data() + end_idx;

            encoded_blocks.resize(pos + value_codec_.max_encoded_size(end_idx - begin_idx));
            if constexpr (delta_encoded) {  // NOLINT
                last_values.push_back(values_[end_idx - 1]);
                pos += value_codec_.delta_encode(begin, end, &encoded_blocks[pos], previous_doc);
                previous_doc = last_values.back();
            } else {
                pos += value_codec_.encode(begin, end, &encoded_blocks[pos]);
            }
        }

        const std::vector<char> encoded_header = irk::encode(int_codec_, {block_size_, num_blocks});
        const std::vector<char> encoded_skips = irk::delta_encode(int_codec_, absolute_skips);

        int list_byte_size = encoded_header.size() + encoded_skips.size() + pos;
        std::vector<char> encoded_last_vals;

        if constexpr (delta_encoded) {  // NOLINT
            encoded_last_vals = irk::delta_encode(value_codec_, last_values);  // NOLINT
            list_byte_size += encoded_last_vals.size();
        }  // NOLINT
        list_byte_size = expanded_size(list_byte_size);  // NOLINT

        auto encoded_list_byte_size = irk::encode(int_codec_, {list_byte_size});
        out.write(&encoded_list_byte_size[0], encoded_list_byte_size.size());
        out.write(&encoded_header[0], encoded_header.size());
        out.write(&encoded_skips[0], encoded_skips.size());
        if constexpr (delta_encoded) {  // NOLINT
            out.write(&encoded_last_vals[0], encoded_last_vals.size());  // NOLINT
        }
        out.write(&encoded_blocks[0], pos);  // NOLINT

        return list_byte_size;
    }

    [[nodiscard]] constexpr auto size() const -> size_type { return values_.size(); }
    [[nodiscard]] constexpr auto values() const -> auto const& { return values_; }

private:
    [[nodiscard]] constexpr auto expanded_size(int list_byte_size) const -> size_type
    {
        unsigned int extra_bytes = 1;
        while (static_cast<unsigned int>(list_byte_size) + extra_bytes
               >= (1u << (extra_bytes * 7u))) {
            extra_bytes++;
        }
        return static_cast<size_type>(list_byte_size + extra_bytes);
    }

    size_type block_size_;
    codec_type value_codec_;
    std::vector<value_type> values_;
    irk::vbyte_codec<int32_t> int_codec_;
};

}  // namespace ir
