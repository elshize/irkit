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

//! \file varbyte.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>
#include <cstdarg>
#include <gsl/span>
#include <iostream>
#include <range/v3/range_concepts.hpp>
#include <range/v3/utility/concepts.hpp>
#include "irkit/types.hpp"
#include "irkit/utils.hpp"

namespace irk::coding {

//! Variable-Byte codec.
template<class T, CONCEPT_REQUIRES_(ranges::Integral<T>())>
struct varbyte_codec {
    using value_type = T;

    //! Encode `n` and write it to `sink`.
    /*!
     * @param n     an integer number to encode
     * @param sink  output stream to write the encoded bytes
     * @returns     the reference to `sink`
     */
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

    //! Decode `source` and write it to `n`.
    /*!
        \param source   input stream to read the encoded bytes from
        \param n        an integer number reference to store the decoded value
        \returns        the reference to `source`
        \throws std::runtime_error whenever the stream ends before finishing
                                     decoding a symbol, unlesss it ends on the
                                     first byte, i.e., there is no more
                                     symbols to decode
     */
    std::istream& decode(std::istream& source, value_type& n) const
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
        if (source.get(b)) {
            if (process_next_byte() != b) {
                return source;
            }
            while (source.get(b)) {
                if (process_next_byte() != b) {
                    return source;
                }
            }
            throw std::runtime_error(
                "reached end of byte range before end of value");
        }
        return source;
    }
};

};  // namespace irk::coding
