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

#include <cstdarg>
#include <iostream>

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>
#include <gsl/span>

namespace irk {

//! Variable-Byte codec.
template<class T>
struct varbyte_codec {
    using value_type = T;

    std::ostream& encode(value_type n, std::ostream& sink) const
    {
        while (true) {
            sink.put(n < 128 ? 128 + n : n % 128);
            if (n < 128) {
                break;
            }
            n /= 128;
        }
        return sink;
    }

    std::streamsize decode(std::istream& source, value_type& n) const
    {
        char b;
        n = 0;
        unsigned short shift = 0;
        auto process_next_byte = [&b, &n, &shift]() {
            int val = b & 0b01111111;
            n |= val << shift;
            shift += 7;
            return val;
        };
        std::streamsize bytes_read = 0;
        if (source.get(b)) {
            bytes_read++;
            if (process_next_byte() != b) {
                return bytes_read;
            }
            while (source.get(b)) {
                bytes_read++;
                if (process_next_byte() != b) {
                    return bytes_read;
                }
            }
            throw std::runtime_error(
                "reached end of byte range before end of value");
        }
        return bytes_read;
    }
};

};  // namespace irk
