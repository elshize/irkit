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

//! \file memorybuffer.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <boost/filesystem.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <vector>

namespace fs = boost::filesystem;

namespace irk::io {

//! A base class for all types of memory buffers.
/*!
    \tparam Block the underlying type
    \author Michal Siedlaczek
 */
class base_memory_buffer {
public:
    using char_type = char;
    virtual char_type* data() = 0;
    virtual const char_type* data() const = 0;
    virtual std::size_t size() const = 0;
};

class vector_memory_buffer : public base_memory_buffer {
    std::vector<char> data_;

public:
    using char_type = char;
    virtual char_type* data() { return data_.data(); }
    virtual const char_type* data() const { return data_.data(); }
    virtual std::size_t size() const { return data_.size(); }
};

class mapped_memory_buffer : public base_memory_buffer {
    boost::iostreams::mapped_file data_;

public:
    using char_type = char;
    mapped_memory_buffer(fs::path file) : data_(file.string()) {}
    virtual char_type* data() { return data_.data(); }
    virtual const char_type* data() const { return data_.data(); }
    virtual std::size_t size() const { return data_.size(); }
};

};  // namespace irk::io
