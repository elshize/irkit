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

#include <algorithm>
#include <bitset>
#include <fstream>
#include <vector>

#include <boost/concept/assert.hpp>
#include <boost/concept_check.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/filesystem.hpp>
#include <gsl/span>

#include <irkit/bitptr.hpp>
#include <irkit/bitstream.hpp>
#include <irkit/coding.hpp>

namespace irk::io {

namespace fs = boost::filesystem;

namespace detail {
    class line : public std::string {
        friend std::istream& operator>>(std::istream& is, line& line)
        {
            return std::getline(is, line);
        }
    };
}  // namespace detail

using line_iterator = std::istream_iterator<detail::line>;

inline void enforce_exist(const fs::path& file)
{
    if (not fs::exists(file)) {
        throw std::invalid_argument("File not found: " + file.generic_string());
    }
}

inline void
load_data(const fs::path& data_file, std::vector<char>& data_container)
{
    enforce_exist(data_file);
    std::ifstream in(data_file.c_str(), std::ios::binary);
    in.seekg(0, std::ios::end);
    std::streamsize size = in.tellg();
    in.seekg(0, std::ios::beg);
    data_container.resize(size);
    if (not in.read(data_container.data(), size)) {
        throw std::runtime_error("Failed reading " + data_file.string());
    }
}

inline void
load_lines(const fs::path& data_file, std::vector<std::string>& lines)
{
    enforce_exist(data_file);
    std::ifstream in(data_file.c_str());
    std::string line;
    int n = 0;
    while (std::getline(in, line)) {
        lines.push_back(line);
        n++;
    }
}

inline std::vector<std::string> load_lines(const fs::path& data_file)
{
    std::vector<std::string> lines;
    load_lines(data_file, lines);
    return lines;
}

//! Appends the underlying bytes of an object to a byte buffer.
template<class T>
void append_object(const T& object, std::vector<char>& buffer)
{
    buffer.insert(buffer.end(),
        reinterpret_cast<const char*>(&object),
        reinterpret_cast<const char*>(&object) + sizeof(object));
}

//! Appends the underlying bytes of a collection to a byte buffer.
template<class Collection>
void append_collection(const Collection& collection, std::vector<char>& buffer)
{
    if (not collection.empty())
    {
        buffer.insert(buffer.end(),
            reinterpret_cast<const char*>(collection.data()),
            reinterpret_cast<const char*>(collection.data())
                + collection.size() * sizeof(collection.front()));
    }
}

};  // namespace irk::io
