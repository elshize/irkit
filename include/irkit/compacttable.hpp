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

//! \file compacttable.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <boost/filesystem.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <fstream>
#include <iostream>
#include "irkit/coding.hpp"
#include "irkit/coding/varbyte.hpp"
#include "irkit/io.hpp"
#include "irkit/io/memorybuffer.hpp"
#include "irkit/types.hpp"

namespace irk {

namespace fs = boost::filesystem;

struct CompactTableHeaderFlags {
    static const std::uint32_t Default = 0;
    static const std::uint32_t DeltaEncoding = 1;
};

struct compact_table_header {
    std::uint32_t count;
    std::uint32_t block_size;
    std::uint32_t flags = CompactTableHeaderFlags::Default;
};

struct compact_table_leader {
    std::uint32_t key;
    std::uint32_t ptr;
    bool operator<(const compact_table_leader& rhs) const
    {
        return key < rhs.key;
    }
};

template<class Mem, class Codec>
std::size_t read_compact_value(Mem mem, std::uint32_t key, Codec codec)
{
    auto header = reinterpret_cast<const compact_table_header*>(mem.data());
    bool delta_encoded = header->flags & CompactTableHeaderFlags::DeltaEncoding;
    auto leader_count =
        (header->count + header->block_size - 1) / header->block_size;
    gsl::span<const compact_table_leader> leaders(
        reinterpret_cast<const compact_table_leader*>(
            mem.data() + sizeof(*header)),
        leader_count);
    auto leader = std::lower_bound(
        leaders.begin(), leaders.end(), compact_table_leader{key, 0});
    if (leader == leaders.end() || leader->key > key) {
        // Legal because the first leader key must be 0.
        leader--;
    }
    const char* block_beg = mem.data() + leader->ptr;
    std::size_t num_skip = key - leader->key;
    auto decoded = delta_encoded
        ? decode_delta_n(
              gsl::span<const char>(block_beg, mem.size() - leader->ptr),
              num_skip + 1,
              codec)
        : decode_n(gsl::span<const char>(block_beg, mem.size() - leader->ptr),
              num_skip + 1,
              codec);
    return decoded.back();
}

template<class T, class Codec, class MemoryBuffer>
class compact_table;

template<class T, class Codec, class MemoryBuffer>
std::ostream& operator<<(std::ostream& out,
    const compact_table<T, Codec, MemoryBuffer>& offset_table)
{
    return out.write(offset_table.data_.data(), offset_table.data_.size());
}

//! Fast-access compressed array.
/*!
    \tparam T               the type of the stored values
    \tparam MemoryBuffer    the memory buffer type, e.g., `std::vector<char>`
                            (for building/reading) or
                            `boost::iostreams::mapped_file` (for reading)
   \tparam Codec            a codec that encodes values of type `T`

    This is a compressed table indexed with consecutive integers between `0` and
    `size - 1`. TODO: describe implmentation.

    \author Michal Siedlaczek
 */
template<class T,
    class Codec = varbyte_codec<T>,
    class MemoryBuffer = std::vector<char>>
class compact_table {
    static_assert(std::is_convertible<T, typename Codec::value_type>::value);

public:
    using value_type = typename Codec::value_type;

protected:
    Codec codec_;
    MemoryBuffer data_;

public:
    explicit compact_table(MemoryBuffer data) : data_(data) {}
    compact_table(compact_table&& other) : data_(std::move(other.data_)) {}
    compact_table(const compact_table& other) : data_(other.data_) {}
    compact_table<T, Codec, MemoryBuffer>&
    operator=(const compact_table<T, Codec, MemoryBuffer>& other)
    {
        data_ = other.data_;
        return *this;
    }

    std::size_t operator[](std::size_t term_id)
    {
        return read_compact_value(data_, term_id, codec_);
    }

    std::size_t operator[](std::size_t term_id) const
    {
        return read_compact_value(data_, term_id, codec_);
    }

    const char* data() { return data_.data(); }

    const compact_table_header* header() const
    {
        return reinterpret_cast<const compact_table_header*>(data_.data());
    };

    std::size_t size() const { return header()->count; }

    bool operator==(const compact_table<T, Codec>& rhs)
    {
        return data_ == rhs.data_;
    }

    friend std::ostream&
    operator<<<>(std::ostream&, const compact_table<T, Codec>&);
};

//! Load a compact table to main memory.
template<class T, class Codec = varbyte_codec<T>>
compact_table<T, Codec, std::vector<char>> load_compact_table(fs::path file)
{
    std::vector<char> data;
    io::load_data(file, data);
    return compact_table<T, Codec>(std::move(data));
}

//! Set up a compact table with a mapped file.
template<class T, class Codec = varbyte_codec<T>>
compact_table<T, Codec, boost::iostreams::mapped_file_source>
map_compact_table(fs::path file)
{
    boost::iostreams::mapped_file_source mf(file);
    return compact_table<T, Codec, boost::iostreams::mapped_file_source>(
        std::move(mf));
}

//! Build a compact table in main memory.
template<class T, class Codec = varbyte_codec<T>>
compact_table<T, Codec, std::vector<char>>
build_compact_table(const std::vector<T>& values,
    bool delta_encoded = false,
    std::uint32_t block_size = 256)
{
    std::vector<char> data;
    Codec codec;
    auto flags = delta_encoded ? CompactTableHeaderFlags::DeltaEncoding
                               : CompactTableHeaderFlags::Default;
    compact_table_header header{
        static_cast<std::uint32_t>(values.size()), block_size, flags};
    io::append_object(header, data);

    std::uint32_t block_count =
        (header.count + header.block_size - 1) / header.block_size;
    std::uint32_t data_offset =
        sizeof(header) + block_count * sizeof(compact_table_leader);

    std::vector<char> blocks;
    std::vector<compact_table_leader> leaders;
    for (std::size_t block = 0; block < block_count; ++block) {
        std::uint32_t beg = block * block_size;
        std::uint32_t end =
            std::min(static_cast<std::size_t>(beg + block_size), values.size());
        leaders.push_back(
            {beg, data_offset + static_cast<std::uint32_t>(blocks.size())});
        gsl::span<const T> block_span(values.data() + beg, values.data() + end);
        auto encoded_block = delta_encoded ? encode_delta(block_span, codec)
                                           : encode(block_span, codec);
        blocks.insert(blocks.end(), encoded_block.begin(), encoded_block.end());
    }
    io::append_collection(leaders, data);
    data.insert(data.end(), blocks.begin(), blocks.end());
    return compact_table<T, Codec>(std::move(data));
}

//! Load an offset table to main memory.
template<class Codec = varbyte_codec<std::size_t>>
compact_table<std::size_t, Codec, std::vector<char>>
load_offset_table(fs::path file)
{
    std::vector<char> data;
    io::load_data(file, data);
    return compact_table<std::size_t, Codec>(std::move(data));
}

//! Set up an offset table with a mapped file.
template<class Codec = varbyte_codec<std::size_t>>
compact_table<std::size_t, Codec, boost::iostreams::mapped_file_source>
map_offset_table(fs::path file)
{
    boost::iostreams::mapped_file_source mf(file);
    return compact_table<std::size_t,
        Codec,
        boost::iostreams::mapped_file_source>(std::move(mf));
}

//! Build an offset table in main memory.
template<class Codec = varbyte_codec<std::size_t>>
compact_table<std::size_t, Codec, std::vector<char>> build_offset_table(
    const std::vector<std::size_t>& values, std::uint32_t block_size = 256)
{
    return build_compact_table<std::size_t, Codec>(values, true, block_size);
}

template<class T, class Codec = varbyte_codec<T>>
using vector_compact_table = compact_table<T, Codec, std::vector<char>>;

template<class T, class Codec = varbyte_codec<T>>
using mapped_compact_table =
    compact_table<T, Codec, boost::iostreams::mapped_file_source>;

template<class Codec = varbyte_codec<std::size_t>,
    class MemoryBuffer = std::vector<char>>
using offset_table = compact_table<std::size_t, Codec, MemoryBuffer>;

template<class Codec = varbyte_codec<std::size_t>>
using vector_offset_table =
    compact_table<std::size_t, Codec, std::vector<char>>;

template<class Codec = varbyte_codec<std::size_t>>
using mapped_offset_table =
    compact_table<std::size_t, Codec, boost::iostreams::mapped_file_source>;

namespace io {

    //! Write a `compact_table` to a file.
    template<class T, class Codec>
    void dump(const compact_table<T, Codec>& offset_table, fs::path file)
    {
        std::ofstream out(file.c_str(), std::ios::binary);
        out << offset_table;
        out.close();
    }

};  // namespace io

};  // namespace irk
