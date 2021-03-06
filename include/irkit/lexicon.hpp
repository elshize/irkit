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

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/range/irange.hpp>

#include <irkit/alphabetical_bst.hpp>
#include <irkit/assert.hpp>
#include <irkit/bitstream.hpp>
#include <irkit/coding/hutucker.hpp>
#include <irkit/coding/prefix_codec.hpp>
#include <irkit/coding/vbyte.hpp>
#include <irkit/io.hpp>
#include <irkit/memoryview.hpp>
#include <irkit/radix_tree.hpp>

namespace irk {

template<class C, class M>
class lexicon {
public:
    using value_type = std::string;
    using reference = value_type&;
    using const_reference = const value_type&;
    using difference_type = std::ptrdiff_t;
    using index_type = std::ptrdiff_t;
    using codec_type = C;
    using memory_container = M;

    lexicon() = delete;
    lexicon(std::vector<std::ptrdiff_t> block_offsets,
        std::vector<index_type> leading_indices,
        memory_container blocks,
        std::ptrdiff_t count,
        int keys_per_block,
        std::shared_ptr<irk::radix_tree<int>> leading_keys,  // NOLINT
        irk::prefix_codec<codec_type> codec)
        : block_offsets_(std::move(block_offsets)),
          leading_indices_(std::move(leading_indices)),
          blocks_(std::move(blocks)),
          count_(count),
          keys_per_block_(keys_per_block),
          leading_keys_(std::move(leading_keys)),
          codec_(std::move(codec))
    {}
    lexicon(const lexicon&) = default;
    lexicon(lexicon&&) noexcept = default;
    lexicon& operator=(const lexicon&) = default;
    lexicon& operator=(lexicon&&) noexcept = delete;
    ~lexicon() = default;

    irk::memory_view block_memory_view(std::size_t block) const
    {
        EXPECTS(block >= 0);
        EXPECTS(block < block_offsets_.size());
        auto block_offset = block_offsets_[block];
        auto next_block_offset = block + 1 < block_offsets_.size()
            ? block_offsets_[block + 1]
            : blocks_.size();
        std::ptrdiff_t size = next_block_offset - block_offset;
        ENSURES(size > 0);
        ENSURES(size <= static_cast<std::ptrdiff_t>(blocks_.size()));
        if constexpr (std::is_same<memory_container,
                                   irk::memory_view>::value)  // NOLINT
        {
            return blocks_.range(block_offset, size);
        }
        else {  // NOLINT
            return irk::make_memory_view(&blocks_[block_offset], size);
        }
    }

    std::optional<index_type> index_at(const std::string& key) const
    {
        auto block = leading_keys_->seek_le(key);
        if (not block.has_value()) { return std::nullopt; }
        auto block_memory = block_memory_view(*block);
        boost::iostreams::stream<boost::iostreams::basic_array_source<char>>
            buffer(block_memory.data(), block_memory.size());
        irk::input_bit_stream bin(buffer);

        index_type value = leading_indices_[*block];
        std::string k;
        codec_.reset();
        codec_.decode(bin, k);
        int idx = 1;
        while (k < key && idx++ < keys_per_block_) {
            codec_.decode(bin, k);
            ++value;
        }
        return k == key ? std::make_optional(value) : std::nullopt;
    }

    std::string key_at(std::ptrdiff_t index) const
    {
        EXPECTS(index < size());
        auto block_pos = std::prev(std::upper_bound(
            leading_indices_.begin(), leading_indices_.end(), index));
        auto block = std::distance(leading_indices_.begin(), block_pos);
        auto block_memory = block_memory_view(block);
        boost::iostreams::stream<boost::iostreams::basic_array_source<char>>
            buffer(block_memory.data(), block_memory.size());
        irk::input_bit_stream bin(buffer);

        index_type value = *block_pos;
        std::string key;
        codec_.reset();
        codec_.decode(bin, key);
        while (value < index) {
            codec_.decode(bin, key);
            ++value;
        }
        return key;
    }

    std::ostream& serialize(std::ostream& out) const
    {
        std::vector<char> header(max_serialized_header_size());
        vbyte_codec<std::ptrdiff_t> intcodec;
        auto header_iter = std::begin(header);

        std::advance(header_iter, intcodec.encode(&count_, header_iter));
        auto size = block_offsets_.size();
        std::advance(header_iter, intcodec.encode(&size, header_iter));
        std::advance(
            header_iter, intcodec.encode(&keys_per_block_, header_iter));

        for (const auto& offset : block_offsets_)
        { std::advance(header_iter, intcodec.encode(&offset, header_iter)); }
        for (const auto& index : leading_indices_)
        { std::advance(header_iter, intcodec.encode(&index, header_iter)); }

        header.resize(std::distance(std::begin(header), header_iter));

        boost::iostreams::stream<
            boost::iostreams::back_insert_device<std::vector<char>>>
            buffer(boost::iostreams::back_inserter(header));

        dump_coding_tree(buffer);
        dump_leading_keys(buffer);
        buffer.flush();

        std::ptrdiff_t header_size = header.size() + sizeof(std::ptrdiff_t);
        out.write(
            reinterpret_cast<char*>(&header_size), sizeof(std::ptrdiff_t));
        out.write(&header[0], header.size());
        out.flush();

        dump_blocks(out);
        return out;
    }

    void serialize(const boost::filesystem::path& file) const
    {
        std::ofstream out(file.c_str());
        serialize(out);
    }

    std::ptrdiff_t size() const { return count_; }
    bool empty() const { return count_ > 0; }

    class iterator : public boost::iterator_facade<
        iterator, const std::string, boost::single_pass_traversal_tag> {
    public:
        iterator(const lexicon<codec_type, memory_container>& lex,
            const memory_container& blocks,
            int block_num,
            int pos_in_block,
            int keys_per_block,
            const prefix_codec<codec_type>& codec)
            : lex_(lex),
              blocks_(blocks),
              block_num_(block_num),
              pos_in_block_(pos_in_block),
              keys_per_block_(keys_per_block),
              codec_(codec)
        {
            decode_block(block_num_, decoded_block_);
        }

        bool operator!=(const iterator& other) const
        {
            return not equal(other);
        }

    private:
        friend class boost::iterator_core_access;

        void increment()
        {
            if (++pos_in_block_ == keys_per_block_)
            {
                pos_in_block_ = 0;
                block_num_++;
                decoded_block_.clear();
                decode_block(block_num_, decoded_block_);
            }
        }

        bool equal(const iterator& other) const
        {
            return block_num_ == other.block_num_
                && pos_in_block_ == other.pos_in_block_;
        }

        const std::string& dereference() const
        {
            return decoded_block_[pos_in_block_];
        }

        void decode_block(int block, std::vector<std::string>& keys) const
        {
            if (block
                >= static_cast<std::ptrdiff_t>(lex_.block_offsets_.size())) {
                return;
            }
            auto block_memory = lex_.block_memory_view(block);
            boost::iostreams::stream<boost::iostreams::basic_array_source<char>>
                buffer(block_memory.data(), block_memory.size());
            irk::input_bit_stream bin(buffer);

            codec_.reset();
            for (index_type idx = 0; idx < keys_per_block_; ++idx)
            {
                std::string key;
                codec_.decode(bin, key);
                keys.push_back(key);
            }
        }

        const lexicon<codec_type, memory_container>& lex_;
        const memory_container& blocks_;
        int block_num_;
        int pos_in_block_;
        int keys_per_block_;
        mutable std::string val_;
        mutable std::vector<std::string> decoded_block_;
        const irk::prefix_codec<codec_type>& codec_;
    };
    using const_iterator = iterator;

    iterator begin() const {
        return iterator(*this, blocks_, 0, 0, keys_per_block_, codec_);
    }

    iterator end() const {
        auto block_count = block_offsets_.size();
        auto pos_in_block = (count_ - leading_indices_.back())
            % keys_per_block_;
        auto block = pos_in_block == 0 ? block_count : block_count - 1;
        return iterator(*this,
            blocks_,
            block,
            pos_in_block,
            keys_per_block_,
            codec_);
    }

    int keys_per_block() const { return keys_per_block_; }

private:
    std::vector<std::ptrdiff_t> block_offsets_;
    std::vector<index_type> leading_indices_;
    memory_container blocks_;
    std::ptrdiff_t count_;
    int keys_per_block_;
    std::shared_ptr<irk::radix_tree<int>> leading_keys_;
    irk::prefix_codec<codec_type> codec_;

    std::ptrdiff_t max_serialized_header_size() const
    {
        auto size = block_offsets_.size();
        return sizeof(count_) + sizeof(size) + sizeof(keys_per_block_)
            + size * sizeof(block_offsets_[0])
            + size * sizeof(leading_indices_[0]);
    }

    std::ostream& dump_coding_tree(std::ostream& out) const
    {
        auto coding_tree = codec_.codec().tree();
        auto mem = coding_tree.memory_container();
        std::size_t tree_size = mem.size();
        out.write(reinterpret_cast<char*>(&tree_size), sizeof(tree_size));
        out.write(mem.data(), tree_size);
        out.flush();
        return out;
    }

    std::ostream& dump_blocks(std::ostream& out) const
    {
        std::size_t blocks_size = blocks_.size();
        out.write(blocks_.data(), blocks_size);
        return out;
    }

    std::ostream& dump_leading_keys(std::ostream& out) const
    {
        output_bit_stream bout(out);
        irk::prefix_codec<codec_type> encoder(codec_.codec());
        for (const auto& block : boost::irange<int>(0, block_offsets_.size()))
        {
            codec_.reset();
            auto buffer = block_memory_view(block).stream();
            irk::input_bit_stream bin(buffer);
            std::string key;
            codec_.decode(bin, key);
            encoder.encode(key, bout);
        }
        bout.flush();
        return out;
    }
};

template<class C>
using lexicon_view = lexicon<C, irk::memory_view>;

inline lexicon_view<hutucker_codec<char>>
load_lexicon(const irk::memory_view& memory)
{
    // Total header size: everything that will be always read to memory.
    auto header_size = memory.range(0, sizeof(ptrdiff_t)).as<std::ptrdiff_t>();
    auto header_memory = memory(sizeof(ptrdiff_t), header_size);
    irk::vbyte_codec<std::ptrdiff_t> intcodec;
    auto header_iter = header_memory.begin();

    // Block metadata
    std::ptrdiff_t block_count, value_count, keys_per_block;
    std::vector<std::ptrdiff_t> block_offsets;
    std::vector<std::ptrdiff_t> leading_indices;
    header_iter = intcodec.decode(header_iter, &value_count);
    header_iter = intcodec.decode(header_iter, &block_count);
    header_iter = intcodec.decode(header_iter, &keys_per_block);
    for (int idx : boost::irange<int>(0, block_count)) {
        (void)idx;
        std::ptrdiff_t offset;
        header_iter = intcodec.decode(header_iter, &offset);
        block_offsets.push_back(offset);
    }
    for (int idx : boost::irange<int>(0, block_count)) {
        (void)idx;
        std::ptrdiff_t first_index;
        header_iter = intcodec.decode(header_iter, &first_index);
        leading_indices.push_back(first_index);
    }

    // Encoding tree
    std::size_t tree_size = *reinterpret_cast<const std::size_t*>(
        &*header_iter);
    std::advance(header_iter, sizeof(std::size_t));
    auto tree_end = std::next(header_iter, tree_size);
    std::vector<char> tree_data(header_iter, tree_end);
    alphabetical_bst<> encoding_tree(std::move(tree_data));
    hutucker_codec<char> ht_codec(std::move(encoding_tree));

    // Block leading values
    boost::iostreams::stream<boost::iostreams::basic_array_source<char>> buffer(
        tree_end,
        header_size - std::distance(header_memory.begin(), tree_end)
            - sizeof(header_size));
    irk::input_bit_stream bin(buffer);
    auto leading_keys = std::make_shared<irk::radix_tree<int>>();
    auto pcodec = irk::prefix_codec<hutucker_codec<char>>(std::move(ht_codec));
    for (int idx : boost::irange<int>(0, block_count)) {
        std::string key;
        pcodec.decode(bin, key);
        leading_keys->insert(key, idx);
    }
    return lexicon_view<hutucker_codec<char>>(std::move(block_offsets),
        std::move(leading_indices),
        memory(header_size, memory.size()),
        value_count,
        keys_per_block,
        leading_keys,
        std::move(pcodec));
}

//! Build a lexicon in memory.
//!
//! \param keys_begin   begin iterator of keys to insert
//! \param keys_end     end iterator of keys to insert
//! \param corpus_begin begin iterator of keys to calculate symbol frequencies
//! \param corpus_end   end iterator of keys to calculate symbol frequencies
//! \param keys_per_block how many keys to store in a single block
//!
//! Typically, both keys and corpus will be the same collection.
//! They are separated mainly for situations when these are single-pass
//! iterators. See overloads for a more convenient interface.
template<class KeyIterator, class CorpusIterator>
lexicon<hutucker_codec<char>, std::vector<char>> build_lexicon(
    KeyIterator keys_begin,
    KeyIterator keys_end,
    CorpusIterator corpus_begin,
    CorpusIterator corpus_end,
    int keys_per_block)
{
    EXPECTS(keys_begin != keys_end);
    EXPECTS(corpus_begin != corpus_end);

    std::vector<std::size_t> frequencies(256, 0);
    for (; corpus_begin != corpus_end; ++corpus_begin)
    {
        for (const char& ch : *corpus_begin)
        { ++frequencies[static_cast<unsigned char>(ch)]; }
    }
    auto codec = hutucker_codec<char>(frequencies);

    std::vector<std::ptrdiff_t> block_offsets;
    std::vector<std::ptrdiff_t> leading_indices;
    auto leading_keys = std::make_shared<irk::radix_tree<int>>();
    std::vector<char> blocks;
    boost::iostreams::stream<
        boost::iostreams::back_insert_device<std::vector<char>>>
        buffer(boost::iostreams::back_inserter(blocks));
    irk::output_bit_stream bout(buffer);
    auto pcodec = irk::prefix_codec<hutucker_codec<char>>(std::move(codec));

    std::ptrdiff_t index = 0;
    int block_idx = 0;
    while (keys_begin != keys_end) {
        block_offsets.push_back(blocks.size());
        leading_indices.push_back(index++);
        std::string leading_key = *keys_begin++;
        leading_keys->insert(leading_key, block_idx);
        pcodec.reset();
        pcodec.encode(leading_key, bout);
        for (int idx_in_block = 1;
             idx_in_block < keys_per_block && keys_begin != keys_end;
             ++idx_in_block, ++index, ++keys_begin) {
            pcodec.encode(*keys_begin, bout);
        }
        ++block_idx;
        bout.flush();
    }
    pcodec.reset();
    return lexicon<irk::hutucker_codec<char>, std::vector<char>>(
        std::move(block_offsets),
        std::move(leading_indices),
        std::move(blocks),
        index,
        keys_per_block,
        leading_keys,
        std::move(pcodec));
}

inline lexicon<hutucker_codec<char>, std::vector<char>>
build_lexicon(const std::vector<std::string>& keys, int keys_per_block)
{
    return build_lexicon(
        keys.begin(), keys.end(), keys.begin(), keys.end(), keys_per_block);
}

inline lexicon<hutucker_codec<char>, std::vector<char>>
build_lexicon(const boost::filesystem::path& file, int keys_per_block)
{
    std::ifstream keys(file.c_str());
    std::ifstream corpus(file.c_str());
    return build_lexicon(
        irk::io::line_iterator(keys),
        irk::io::line_iterator(),
        irk::io::line_iterator(corpus),
        irk::io::line_iterator(),
        keys_per_block);
}

}  // namespace irk
