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

#include <bitset>
#include <cstdarg>
#include <iostream>
#include <iterator>

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>
#include <gsl/span>
#include <type_safe/types.hpp>

#include <irkit/coding/stream_vbyte.hpp>

namespace irk {

namespace ts = type_safe;

template<class T>
struct vbyte_codec {
    using value_type = T;
    static constexpr std::uint8_t maxv = 128;

    std::ptrdiff_t max_encoded_size(
        int count, std::optional<T> /* max_value */ = std::nullopt) const
    {
        return streamvbyte_max_compressedbytes(count);
    }

    template<class InputIterator, class OutputIterator>
    std::ptrdiff_t encode(InputIterator in, OutputIterator out) const
    {
        using iterator_type = std::iterator_traits<InputIterator>;
        using value_type =
            std::make_unsigned_t<typename iterator_type::value_type>;
        auto v = static_cast<value_type>(*in);
        std::ptrdiff_t size = 0;
        while (true)
        {
            auto last_byte = v < maxv;
            *out = v % maxv + maxv * last_byte;
            ++out;
            ++size;
            if (last_byte) { break; }
            v /= maxv;
        }
        return size;
    }

    template<class InputIterator, class OutputIterator>
    std::ptrdiff_t
    encode(InputIterator lo, InputIterator hi, OutputIterator out) const
    {
        std::ptrdiff_t size = 0;
        for (; lo != hi; ++lo) { size += encode(lo, std::next(out, size)); }
        return size;
    }

    template<class InputIterator, class OutputIterator>
    std::ptrdiff_t delta_encode(InputIterator lo,
        InputIterator hi,
        OutputIterator out,
        T initial = T()) const
    {
        std::ptrdiff_t size = 0;
        for (; lo != hi; ++lo)
        {
            T val = *lo - initial;
            size += encode(&val, std::next(out, size));
            initial = *lo;
        }
        return size;
    }

    template<class InputIterator, class OutputIterator>
    InputIterator decode(InputIterator in, OutputIterator out) const
    {
        using iterator_type = std::iterator_traits<OutputIterator>;
        using value_type =
            std::make_unsigned_t<typename iterator_type::value_type>;
        unsigned char b;
        std::size_t n = 0;
        unsigned int shift = 0;

        auto process_next_byte = [&b, &n, &shift]() -> std::size_t {
            std::size_t val = b & 0b01111111u;
            n |= val << shift;
            shift += 7;
            return val;
        };

        b = static_cast<unsigned char>(*in);
        ++in;
        while (process_next_byte() == b)
        {
            b = static_cast<unsigned char>(*in);
            ++in;
        }
        *out = static_cast<typename iterator_type::value_type>(n);
        return in;
    }

    template<class InputIterator, class OutputIterator>
    InputIterator
    decode(InputIterator in, OutputIterator out, int n) const
    {
        for (int idx = 0; idx < n; ++idx) { in = decode(in, out++); }
        return in;
    }

    template<class InputIterator, class OutputIterator>
    InputIterator delta_decode(
        InputIterator in, OutputIterator out, int n, T initial = T()) const
    {
        for (int idx = 0; idx < n; ++idx)
        {
            in = decode(in, out);
            *out += initial;
            initial = *out;
            ++out;
        }
        return in;
    }
};

};  // namespace irk
