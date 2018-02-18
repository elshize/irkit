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

//! \file prefixmap.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <boost/dynamic_bitset.hpp>
#include <cstring>
#include <iostream>
#include <list>
#include <vector>
#include "irkit/bitptr.hpp"
#include "irkit/coding/hutucker.hpp"
#include "irkit/mutablebittrie.hpp"
#include "irkit/utils.hpp"

namespace irk {

//! A string-based prefix map implementation.
/*!
     \tparam Index        the integral type of element indices
     \tparam MemoryBuffer the memory buffer type, e.g., `std::vector<char>` (for
                          building/reading) or `boost::iostreams::mapped_file`
                          (for reading)

     This represents a string-based prefix map, which allows for the following:
      - determinig whether a string is indexed in the map;
      - returning a string's ID (lexicographical position) if it exists;
      - returning the n-th string in lexicographical order;
      - [TODO: intervals]

      \author Michal Siedlaczek
 */
template<class Index, class MemoryBuffer, class Counter = std::uint32_t>
class prefix_map {
    static constexpr std::size_t block_data_offset =
        sizeof(Index) + sizeof(Counter);

public:
    /*!
     * An object of this class is responsible for building a single compressed
     * block of a prefix map.
     */
    class block_builder {
    private:
        Index first_index_;
        Counter count_;
        std::size_t pos_;
        std::string last_;
        std::size_t size_;
        char* block_begin_;
        irk::bitptr<char> bitp_;
        const std::shared_ptr<irk::coding::hutucker_codec<char>> codec_;

        void encode_unary(std::uint32_t n)
        {
            for (std::uint32_t idx = 0; idx < n; ++idx) {
                bitp_.set(1);
                ++bitp_;
            }
            bitp_.set(0);
            ++bitp_;
            pos_ += n + 1;
        }

        bool can_encode(std::uint32_t length) const
        {
            return pos_ + length <= size_ * 8;
        }

    public:
        block_builder(Index first_index,
            char* block_begin,
            std::size_t size,
            const std::shared_ptr<irk::coding::hutucker_codec<char>> codec)
            : first_index_(first_index),
              count_(0),
              pos_(block_data_offset * 8),
              last_(""),
              size_(size),
              block_begin_(block_begin),
              bitp_(block_begin + block_data_offset),
              codec_(codec)
        {
            std::memcpy(block_begin, &first_index, sizeof(Index));
        }

        block_builder(block_builder&& other)
            : first_index_(other.first_index_),
              count_(other.count_),
              pos_(other.pos_),
              last_(other.last_),
              size_(other.size_),
              bitp_(other.bitp_),
              codec_(other.codec_)
        {}

        bool add(const std::string& value)
        {
            std::uint32_t pos = 0;
            for (; pos < last_.size(); ++pos) {
                if (last_[pos] != value[pos]) {
                    break;
                }
            }
            auto encoded = codec_->encode(value.begin() + pos, value.end());
            if (!can_encode(value.size() + 2 + encoded.size())) {
                return false;
            }
            encode_unary(pos);
            encode_unary(value.size() - pos);
            irk::bitcpy(bitp_, encoded);
            bitp_ += encoded.size();
            pos_ += encoded.size();
            count_++;
            last_ = value;
            return true;
        }

        void write_count()
        {
            std::memcpy(block_begin_ + sizeof(Index), &count_, sizeof(Counter));
        }
    };

    class block_ptr {
    private:
        const char* block_begin;
        bitptr<const char> current;
        const std::shared_ptr<irk::coding::hutucker_codec<char>> codec_;
        std::string last_value;

    public:
        block_ptr(const char* block_ptr,
            const std::shared_ptr<irk::coding::hutucker_codec<char>> codec)
            : block_begin(block_ptr),
              current(block_ptr + block_data_offset),
              codec_(codec),
              last_value()
        {}

        Index first_index() const
        {
            return *reinterpret_cast<const Index*>(block_begin);
        }

        Counter count() const
        {
            return *reinterpret_cast<const Counter*>(
                block_begin + sizeof(Index));
        }

        std::size_t read_unary()
        {
            std::size_t val = 0;
            while (*current) {
                ++val;
                ++current;
            }
            ++current;
            return val;
        }

        std::string next()
        {
            std::size_t common_prefix_len = read_unary();
            std::size_t suffix_len = read_unary();
            last_value.resize(common_prefix_len);
            std::ostringstream o(std::move(last_value), std::ios_base::ate);
            auto reader = current.reader();
            codec_->decode(reader, o, suffix_len);
            last_value = o.str();
            return last_value;
        }
    };

private:
    MemoryBuffer blocks_;
    std::size_t block_size_;
    std::size_t block_count_;
    const std::shared_ptr<irk::coding::hutucker_codec<char>> codec_;
    mutable_bit_trie<Index> block_leaders_;

    //! Append another block
    std::shared_ptr<block_builder>
    append_block(Index index, block_builder* old_builder)
    {
        if (old_builder != nullptr) {
            old_builder->write_count();
        }
        blocks_.resize(blocks_.size() + block_size_);
        ++block_count_;
        char* new_block_begin = blocks_.data() + blocks_.size() - block_size_;
        return std::make_shared<block_builder>(
            index, new_block_begin, block_size_, codec_);
    }

public:
    template<class StringRange>
    prefix_map(StringRange items,
        const std::shared_ptr<irk::coding::hutucker_codec<char>> codec,
        std::size_t block_size = 1024)
        : block_size_(block_size), block_count_(0), codec_(codec)
    {
        auto it = items.cbegin();
        auto last = items.cend();
        if (it == last) {
            throw std::invalid_argument("prefix map cannot be empty");
        }

        Index index(0);
        std::string item = *it;
        block_leaders_.insert(codec_->encode(item.begin(), item.end()), 0);
        auto current_block = append_block(index, nullptr);

        for (; it != last; ++it) {
            std::string item = *it;
            if (!current_block->add(item)) {
                block_leaders_.insert(
                    codec_->encode(item.begin(), item.end()), block_count_);
                current_block = append_block(index, current_block.get());
                if (!current_block->add(item)) {
                    throw std::runtime_error(
                        "strings longer than blocks are not supported");
                }
            }
            ++index;
        }
        current_block->write_count();
    }

    std::optional<Index> operator[](const std::string& key) const
    {
        auto encoded = codec_->encode(key.cbegin(), key.cend());
        auto[exact, block_node] = block_leaders_.find_or_first_lower(encoded);
        if (block_node == nullptr) {
            return std::nullopt;
        }
        std::size_t block_number = block_node->value.value();
        block_ptr block{blocks_.data() + block_number * block_size_, codec_};
        Index idx = block.first_index();
        if (exact) {
            return std::make_optional(block.first_index());
        }
        std::string v = block.next();
        std::uint32_t c = 1;
        while (c < block.count() && v < key) {
            v = block.next();
            ++idx;
            ++c;
        }
        return v == key ? std::make_optional(idx) : std::nullopt;
    }
};

template<class Index, class MemoryBuffer, class StringRange>
prefix_map<Index, MemoryBuffer>
build_prefix_map(StringRange items, std::size_t buffer_size = 1024)
{
    std::vector<std::size_t> frequencies(256, 0);
    for (const std::string& item : items) {
        assert(item.size() > 0);
        for (const char& ch : item) {
            ++frequencies[ch];
        }
    }
    auto codec =
        std::make_shared<irk::coding::hutucker_codec<char>>(frequencies);
    return prefix_map<Index, MemoryBuffer>(items, codec, buffer_size);
}

};  // namespace irk
