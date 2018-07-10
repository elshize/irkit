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
#include <irkit/io.hpp>
#include <irkit/memoryview.hpp>
#include <irkit/radix_tree.hpp>

namespace irk {

template<class C, class M>
class lexicon {
public:
    using value_type = std::ptrdiff_t;
    using codec_type = C;
    using memory_container = M;

    lexicon() = delete;
    lexicon(std::vector<std::ptrdiff_t>&& block_offsets,
        std::vector<value_type>&& leading_indices,
        memory_container&& blocks,
        std::ptrdiff_t count,
        int keys_per_block,
        std::shared_ptr<irk::radix_tree<int>> leading_keys,  // NOLINT
        irk::prefix_codec<codec_type> codec)
        : block_offsets_(block_offsets),
          leading_indices_(leading_indices),
          blocks_(blocks),
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

    irk::memory_view block_memory_view(int block) const
    {
        EXPECTS(block >= 0);
        EXPECTS(block < block_offsets_.size());
        auto block_offset = block_offsets_[block];
        auto next_block_offset = block + 1 < block_offsets_.size()
            ? block_offsets_[block + 1]
            : blocks_.size();
        std::ptrdiff_t size = next_block_offset - block_offset;
        ENSURES(size > 0);
        ENSURES(size <= blocks_.size());
        if constexpr (std::is_same<memory_container,
                                   irk::memory_view>::value)  // NOLINT
        {
            return blocks_.range(block_offset, size);
        }
        else {  // NOLINT
            return irk::make_memory_view(&blocks_[block_offset], size);
        }
    }

    std::optional<value_type> index_at(const std::string& key) const
    {
        auto block = leading_keys_->seek_le(key);
        if (not block.has_value()) { return std::nullopt; }
        auto block_memory = block_memory_view(*block);
        boost::iostreams::stream<boost::iostreams::basic_array_source<char>>
            buffer(block_memory.data(), block_memory.size());
        irk::input_bit_stream bin(buffer);

        value_type value = leading_indices_[*block];
        std::string k;
        codec_.reset();
        codec_.decode(bin, k);
        while (k < key) {
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

        value_type value = *block_pos;
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
        std::vector<char> header;
        boost::iostreams::stream<
            boost::iostreams::back_insert_device<std::vector<char>>>
            buffer(boost::iostreams::back_inserter(header));
        varbyte_codec<std::ptrdiff_t> intcodec;

        intcodec.encode(count_, buffer);
        intcodec.encode(block_offsets_.size(), buffer);
        intcodec.encode(keys_per_block_, buffer);

        for (const auto& offset : block_offsets_)
        { intcodec.encode(offset, buffer); }
        for (const auto& index : leading_indices_)
        { intcodec.encode(index, buffer); }

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

        //void advance(long n)
        //{ for (int idx = 0; idx < n; idx++) { increment(); } }

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
            std::cout << "decoding block " << block << std::endl;
            auto block_memory = lex_.block_memory_view(block);
            boost::iostreams::stream<boost::iostreams::basic_array_source<char>>
                buffer(block_memory.data(), block_memory.size());
            irk::input_bit_stream bin(buffer);

            codec_.reset();
            for (value_type idx = 0; idx < keys_per_block_; ++idx)
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
            block_offsets_.size() - 1,
            pos_in_block,
            keys_per_block_,
            codec_);
    }

private:
    std::vector<std::ptrdiff_t> block_offsets_;
    std::vector<value_type> leading_indices_;
    memory_container blocks_;
    std::ptrdiff_t count_;
    int keys_per_block_;
    std::shared_ptr<irk::radix_tree<int>> leading_keys_;
    irk::prefix_codec<codec_type> codec_;

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
    irk::varbyte_codec<std::ptrdiff_t> intcodec;
    auto header_stream = header_memory.stream();

    // Block metadata
    std::ptrdiff_t block_count, value_count, keys_per_block;
    std::vector<std::ptrdiff_t> block_offsets;
    std::vector<std::ptrdiff_t> leading_indices;
    intcodec.decode(header_stream, value_count);
    intcodec.decode(header_stream, block_count);
    intcodec.decode(header_stream, keys_per_block);
    for (int idx : boost::irange<int>(0, block_count)) {
        (void)idx;
        std::ptrdiff_t offset;
        intcodec.decode(header_stream, offset);
        block_offsets.push_back(offset);
    }
    for (int idx : boost::irange<int>(0, block_count)) {
        (void)idx;
        std::ptrdiff_t first_index;
        intcodec.decode(header_stream, first_index);
        leading_indices.push_back(first_index);
    }

    // Encoding tree
    std::size_t tree_size;
    header_stream.read(reinterpret_cast<char*>(&tree_size), sizeof(tree_size));
    std::vector<char> tree_data(tree_size);
    header_stream.read(tree_data.data(), tree_size);
    alphabetical_bst<> encoding_tree(tree_data);
    hutucker_codec<char> ht_codec(encoding_tree);

    // Block leading values
    irk::input_bit_stream bin(header_stream);
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
template<class KeyIterator>
lexicon<hutucker_codec<char>, std::vector<char>> build_lexicon(
    KeyIterator keys_begin,
    KeyIterator keys_end,
    KeyIterator corpus_begin,
    KeyIterator corpus_end,
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
        for (std::size_t idx_in_block = 1;
             idx_in_block < keys_per_block && keys_begin != keys_end;
             ++idx_in_block, ++index, ++keys_begin)
        {
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

};  // namespace irk
