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

#include <algorithm>
#include <cstring>
#include <iostream>
#include <list>
#include <sstream>
#include <vector>

#include <boost/dynamic_bitset.hpp>
#include <boost/filesystem.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <gsl/gsl>

#include <irkit/bitptr.hpp>
#include <irkit/coding/hutucker.hpp>
#include <irkit/memoryview.hpp>
#include <irkit/radix_tree.hpp>
#include <irkit/utils.hpp>

namespace irk {

namespace fs = boost::filesystem;

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
        const std::shared_ptr<coding::hutucker_codec<char>> codec_;

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

        bool add(const std::string& value)
        {
            assert(value.size() > 0);
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

        void expand_by(std::size_t nbytes) { size_ += nbytes; }

        void reset(char* new_begin)
        {
            block_begin_ = new_begin;
            bitp_ = bitptr(block_begin_ + block_data_offset);
        }

        auto size() const { return size_; }

        void write_count()
        {
            std::memcpy(block_begin_ + sizeof(Index), &count_, sizeof(Counter));
        }

        void close() { pos_ = size_ * 8; }
    };

    class block_ptr {
    private:
        const char* block_begin;
        bitptr<const char> current;
        std::shared_ptr<irk::coding::hutucker_codec<char>> codec_;
        std::string last_value;

    public:
        block_ptr(const char* block_ptr,
            const std::shared_ptr<irk::coding::hutucker_codec<char>> codec)
            : block_begin(block_ptr),
              current(block_ptr + block_data_offset),
              codec_(codec),
              last_value()
        {}

        block_ptr(const block_ptr& other) = default;
        block_ptr& operator=(const block_ptr& other) = default;

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

    class iterator : public boost::iterator_facade<
        iterator, const std::string, boost::single_pass_traversal_tag> {
    public:
        iterator(const MemoryBuffer& blocks,
            int block_size,
            block_ptr block,
            int block_num,
            int pos_in_block,
            const std::shared_ptr<coding::hutucker_codec<char>> codec)
            : blocks_(blocks),
              block_size_(block_size),
              block_(block),
              block_num_(block_num),
              pos_in_block_(pos_in_block),
              codec_(codec)
        {}

    private:
        friend class boost::iterator_core_access;
        void increment()
        {
            if (++pos_in_block_ == block_.count()) {
                block_ = block_ptr(
                    blocks_.data() + (++block_num_) * block_size_, codec_);
                pos_in_block_ = 0;
            }
        }
        void advance(long n)
        { for (int idx = 0; idx < n; idx++) { increment(); } }
        bool equal(const iterator& other) const
        {
            return block_num_ == other.block_num_
                && pos_in_block_ == other.pos_in_block_;
        }
        const std::string& dereference() const
        {
            val_ = block_.next();
            return val_;
        }

        const MemoryBuffer& blocks_;
        mutable block_ptr block_;
        int block_size_;
        int block_num_;
        int pos_in_block_;
        mutable std::string val_;
        const std::shared_ptr<coding::hutucker_codec<char>> codec_;
    };
    using const_iterator = iterator;

private:
    MemoryBuffer blocks_;
    std::size_t size_;
    std::size_t block_size_;
    std::size_t block_count_;
    const std::shared_ptr<coding::hutucker_codec<char>> codec_;
    std::shared_ptr<radix_tree<Index>> block_leaders_;
    mutable std::vector<Index> reverse_lookup_;

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

    void expand_block(block_builder* block)
    {
        blocks_.resize(blocks_.size() + block_size_);
        ++block_count_;
        block->expand_by(block_size_);
        char* new_begin_ptr = blocks_.data() + blocks_.size() - block->size();
        block->reset(new_begin_ptr);
    }

    std::ostream& dump_coding_tree(std::ostream& out) const
    {
        auto coding_tree = codec_->tree();
        auto mem = coding_tree.memory_container();
        std::size_t tree_size = mem.size();
        out.write(reinterpret_cast<char*>(&tree_size), sizeof(tree_size));
        out.write(mem.data(), tree_size);
        return out;
    }

    std::ostream& dump_blocks(std::ostream& out) const
    {
        std::size_t blocks_size = blocks_.size();
        out.write(reinterpret_cast<char*>(&blocks_size), sizeof(blocks_size));
        out.write(blocks_.data(), blocks_size);
        return out;
    }

    std::ostream& dump_leaders(std::ostream& out) const
    {
        raxIterator iter;
        raxStart(&iter, block_leaders_->c_rax());
        int result = raxSeek(&iter, "^", NULL, 0);
        if (result == 0) {
            throw std::bad_alloc();
        }
        std::string last;
        std::vector<Index> values;
        std::vector<std::string> keys;
        while(raxNext(&iter)) {
            std::string key(iter.key, iter.key + iter.key_len);
            values.push_back(*reinterpret_cast<Index*>(iter.data));
            keys.push_back(std::move(key));
        }
        auto num_values = values.size();
        out.write(reinterpret_cast<char*>(&num_values), sizeof(num_values));
        out.write(reinterpret_cast<char*>(values.data()),
            values.size() * sizeof(Index));
        for (const std::string& key : keys) {
            out << key << '\n';
        }
        raxStop(&iter);
        return out;
    }

    void init_reverse_lookup() const
    {
        for(long block_num = 0; block_num < block_count_; block_num++)
        {
            auto block = block_ptr(
                blocks_.data() + block_num * block_size_, codec_);
            reverse_lookup_.push_back(block.first_index());
            std::cout << block.first_index() << " ";
        }
    }

public:
    prefix_map(MemoryBuffer blocks,
        std::size_t size,
        std::size_t block_size,
        std::size_t block_count,
        const std::shared_ptr<irk::coding::hutucker_codec<char>> codec,
        std::shared_ptr<radix_tree<Index>> block_leaders)
        : blocks_(blocks),
          size_(size),
          block_size_(block_size),
          block_count_(block_count),
          codec_(codec),
          block_leaders_(block_leaders)
    {}

    // TODO: get rid of duplication -- leaving for testing now
    prefix_map(fs::path file,
        const std::shared_ptr<irk::coding::hutucker_codec<char>> codec,
        std::size_t block_size = 1024)
        : size_(0),
          block_size_(block_size),
          block_count_(0),
          codec_(codec),
          block_leaders_(new radix_tree<Index>())
    {
        std::ifstream in(file.c_str());
        Index index(0);
        std::string item;
        if (!std::getline(in, item)) {
            throw std::invalid_argument("prefix map cannot be empty");
        }
        block_leaders_->insert(item, index);
        auto current_block = append_block(index, nullptr);
        if (!current_block->add(item)) {
            throw std::runtime_error("TODO: first item too long; feature pending");
        }
        index++;

        while (std::getline(in, item)) {
            if (!current_block->add(item)) {
                block_leaders_->insert(item, block_count_);
                current_block = append_block(index, current_block.get());
                if (!current_block->add(item)) {
                    while (!current_block->add(item)) {
                        expand_block(current_block.get());
                    }
                    current_block->close();
                }
            }
            ++index;
        }
        size_ = index;
        current_block->write_count();
        in.close();
    }

    template<class StringRange>
    prefix_map(const StringRange& items,
        const std::shared_ptr<irk::coding::hutucker_codec<char>> codec,
        std::size_t block_size = 1024)
        : size_(0),
          block_size_(block_size),
          block_count_(0),
          codec_(codec),
          block_leaders_(new radix_tree<Index>())
    {
        auto it = items.cbegin();
        auto last = items.cend();
        if (it == last) {
            throw std::invalid_argument("prefix map cannot be empty");
        }

        Index index(0);
        std::string item(it->begin(), it->end());
        block_leaders_->insert(item, 0);
        auto current_block = append_block(index, nullptr);

        for (; it != last; ++it) {
            std::string item(it->begin(), it->end());
            if (!current_block->add(item)) {
                block_leaders_->insert(item, block_count_);
                current_block = append_block(index, current_block.get());
                if (!current_block->add(item)) {
                    while (!current_block->add(item)) {
                        expand_block(current_block.get());
                    }
                    current_block->close();
                }
            }
            ++index;
        }
        size_ = index;
        current_block->write_count();
    }

    std::optional<Index> operator[](const std::string& key) const
    {
        auto block_opt = block_leaders_->seek_le(key);
        if (!block_opt.has_value()) {
            return std::nullopt;
        }
        std::size_t block_number = block_opt.value();
        block_ptr block{blocks_.data() + block_number * block_size_, codec_};
        Index idx = block.first_index();
        std::string v = block.next();
        std::uint32_t c = 1;
        while (c < block.count() && v < key) {
            v = block.next();
            ++idx;
            ++c;
        }
        return v == key ? std::make_optional(idx) : std::nullopt;
    }

    std::string operator[](const Index& val) const
    {
        assert(val < size_);
        if (reverse_lookup_.empty()) { init_reverse_lookup(); }
        auto block_pos = std::prev(std::upper_bound(
            reverse_lookup_.begin(), reverse_lookup_.end(), val));
        auto block_number = std::distance(reverse_lookup_.begin(), block_pos);
        block_ptr block{blocks_.data() + block_number * block_size_, codec_};
        Index idx = block.first_index();
        std::string v = block.next();
        std::uint32_t c = 1;
        while (c < block.count() && idx < val) {
            v = block.next();
            ++idx;
            ++c;
        }
        return v;
    }

    std::size_t size() const { return size_; }

    iterator begin() const
    {
        return iterator(blocks_,
            block_size_,
            block_ptr(blocks_.data(), codec_),
            0,
            0,
            codec_);
    }

    iterator end() const
    {
        block_ptr last_block(
            blocks_.data() + block_size_ * (block_count_ - 1), codec_);
        auto pos = (size_ - last_block.first_index()) % last_block.count();
        auto block_num = pos == 0 ? block_count_ : block_count_ - 1;
        return iterator(blocks_,
            block_size_,
            last_block,
            block_num,
            pos,
            codec_);
    }

    std::ostream& dump(std::ostream& out) const
    {
        out.write(reinterpret_cast<const char*>(&size_), sizeof(size_));
        out.write(
            reinterpret_cast<const char*>(&block_size_), sizeof(block_size_));
        out.write(
            reinterpret_cast<const char*>(&block_count_), sizeof(block_count_));
        dump_coding_tree(out);
        dump_leaders(out);
        dump_blocks(out);
        return out;
    }
};

template<class Index>
prefix_map<Index, std::vector<char>>
build_prefix_map_from_file(fs::path file, std::size_t buffer_size = 1024)
{
    std::ifstream in(file.c_str());
    std::vector<std::size_t> frequencies(256, 0);
    std::string item;
    while (std::getline(in, item)) {
        for (const char& ch : item) {
            ++frequencies[static_cast<unsigned char>(ch)];
        }
    }
    in.close();
    auto codec = std::shared_ptr<irk::coding::hutucker_codec<char>>(
        new irk::coding::hutucker_codec<char>(frequencies));
    return prefix_map<Index, std::vector<char>>(file, codec, buffer_size);
}

template<class Index, class StringRange>
prefix_map<Index, std::vector<char>>
build_prefix_map(const StringRange& items, std::size_t buffer_size = 1024)
{
    std::vector<std::size_t> frequencies(256, 0);
    for (const std::string& item : items) {
        assert(item.size() > 0);
        for (const char& ch : item) {
            ++frequencies[ch];
        }
    }
    auto codec = std::shared_ptr<irk::coding::hutucker_codec<char>>(
        new irk::coding::hutucker_codec<char>(frequencies));
    return prefix_map<Index, std::vector<char>>(items, codec, buffer_size);
}

template<class Index>
std::shared_ptr<radix_tree<Index>> load_radix_tree(std::istream& in,
    std::size_t block_size,
    std::shared_ptr<irk::coding::hutucker_codec<char>> codec)
{
    std::shared_ptr<radix_tree<Index>> rt(new radix_tree<Index>());
    std::size_t num_values;

    in.read(reinterpret_cast<char*>(&num_values), sizeof(num_values));
    std::vector<Index> values(num_values);
    in.read(reinterpret_cast<char*>(values.data()),
        num_values * sizeof(Index));

    std::string key;
    for (std::size_t idx = 0; idx < num_values; idx++) {
        std::getline(in, key);
        rt->insert(key, values[idx]);
    }
    return rt;
}

template<class Index>
prefix_map<Index, std::vector<char>> load_prefix_map(std::istream& in)
{
    std::size_t size, block_size, block_count, tree_size, block_data_size;

    in.read(reinterpret_cast<char*>(&size), sizeof(size));
    in.read(reinterpret_cast<char*>(&block_size), sizeof(block_size));
    in.read(reinterpret_cast<char*>(&block_count), sizeof(block_count));
    in.read(reinterpret_cast<char*>(&tree_size), sizeof(tree_size));

    std::vector<char> tree_data(tree_size);
    in.read(tree_data.data(), tree_size);
    alphabetical_bst<> encoding_tree(std::move(tree_data));
    auto codec =
        std::make_shared<irk::coding::hutucker_codec<char>>(encoding_tree);

    auto block_leaders = load_radix_tree<Index>(in, block_size, codec);

    in.read(reinterpret_cast<char*>(&block_data_size), sizeof(block_data_size));
    std::vector<char> blocks(block_data_size);
    in.read(blocks.data(), block_data_size);
    return prefix_map<Index, std::vector<char>>(
        std::move(blocks), size, block_size, block_count, codec, block_leaders);
}

template<class Index>
prefix_map<Index, std::vector<char>> load_prefix_map(memory_view mem)
{
    boost::iostreams::stream<boost::iostreams::basic_array_source<char>> in(
        mem.data(), mem.size());
    return load_prefix_map<Index>(in);
}

template<class Index>
prefix_map<Index, std::vector<char>> load_prefix_map(const std::string& file)
{
    std::ifstream in(file, std::ios::binary);
    auto map = load_prefix_map<Index>(in);
    in.close();
    return map;
}

namespace io {

    template<class Index, class MemoryBuffer, class Counter>
    void
    dump(const prefix_map<Index, MemoryBuffer, Counter>& map, fs::path file)
    {
        std::ofstream out(file.c_str(), std::ios::binary);
        map.dump(out);
        out.close();
    }
};

};  // namespace irk
