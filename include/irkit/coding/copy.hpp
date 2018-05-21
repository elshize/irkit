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

//! \file copy.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <irkit/types.hpp>
#include <irkit/utils.hpp>

namespace irk::coding {

//! A codec that simply copies memory as is, no compression.
/*!
 * Mainly for testing purposes.
 */
template<class T>
struct copy_codec {
    using value_type = T;

    //! Encode `n` and write it to `sink`.
    /*!
     * @param n     an integer number to encode
     * @param sink  output stream to write the encoded bytes
     * @returns     the reference to `sink`
     */
    std::ostream& encode(value_type n, std::ostream& sink) const
    {
        sink.write(reinterpret_cast<char*>(&n), sizeof(n));
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
    std::streamsize decode(std::istream& source, value_type& n) const
    {
        if (!source.read(reinterpret_cast<char*>(&n), sizeof(n))) { return 0; }
        return sizeof(n);
    }
};

};  // namespace irk::coding
