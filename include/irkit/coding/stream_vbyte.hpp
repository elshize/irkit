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

#include <cstdint>
#include <iterator>
#include <optional>

#include <streamvbyte.h>
#include <streamvbytedelta.h>

namespace irk {

template<class T>
struct stream_vbyte_codec {
    using value_type = T;
    static constexpr int byte_size = sizeof(std::uint8_t);
    static constexpr int int_size = sizeof(std::uint32_t);

    template<class Iter>
    using value_type_of = typename std::iterator_traits<Iter>::value_type;

    template<class Iter>
    using is_output_iterator =
        std::is_same<typename std::iterator_traits<Iter>::iterator_category,
            std::output_iterator_tag>;

    std::ptrdiff_t
    max_encoded_size(int count, std::optional<T> max_value = std::nullopt) const
    {
        return streamvbyte_max_compressedbytes(count);
    }

    template<class InputIterator, class OutputIterator>
    std::ptrdiff_t encode(InputIterator in, OutputIterator out) const
    {
        static_assert(!is_output_iterator<OutputIterator>(),
            "stream_vbyte_codec does not support back inserters");
        static_assert(sizeof(value_type_of<InputIterator>) == int_size);
        static_assert(sizeof(value_type_of<OutputIterator>) == byte_size);
        auto inptr = reinterpret_cast<std::uint32_t*>(&*in);
        auto outptr = reinterpret_cast<std::uint8_t*>(&*out);
        std::size_t size = streamvbyte_encode(inptr, 1, outptr);
        return size;
    }

    template<class InputIterator, class OutputIterator>
    std::ptrdiff_t
    encode(InputIterator lo, InputIterator hi, OutputIterator out) const
    {
        static_assert(!is_output_iterator<OutputIterator>(),
            "stream_vbyte_codec does not support back inserters");
        static_assert(sizeof(value_type_of<InputIterator>) == int_size);
        static_assert(sizeof(value_type_of<OutputIterator>) == byte_size);
        // TODO: change when the following is resolved:
        //       https://github.com/lemire/streamvbyte/issues/24
        auto inptr = const_cast<std::uint32_t*>(  // NOLINT
            reinterpret_cast<const std::uint32_t*>(&*lo));
        auto outptr = reinterpret_cast<std::uint8_t*>(&*out);
        std::size_t size = streamvbyte_encode(
            inptr, std::distance(lo, hi), outptr);
        return size;
    }

    template<class InputIterator, class OutputIterator>
    std::ptrdiff_t delta_encode(InputIterator lo,
        InputIterator hi,
        OutputIterator out,
        T initial = T()) const
    {
        static_assert(!is_output_iterator<OutputIterator>(),
            "stream_vbyte_codec does not support back inserters");
        static_assert(sizeof(value_type_of<InputIterator>) == int_size);
        static_assert(sizeof(value_type_of<OutputIterator>) == byte_size);
        // TODO: change when the following is resolved:
        //       https://github.com/lemire/streamvbyte/issues/24
        auto inptr = const_cast<std::uint32_t*>(  // NOLINT
            reinterpret_cast<const std::uint32_t*>(&*lo));
        auto outptr = reinterpret_cast<std::uint8_t*>(&*out);
        std::size_t size = streamvbyte_delta_encode(inptr,
            std::distance(lo, hi),
            outptr,
            static_cast<std::uint32_t>(initial));
        return size;
    }

    template<class InputIterator, class OutputIterator>
    InputIterator decode(InputIterator in, OutputIterator out) const
    {
        return decode(in, out, 1);
    }

    template<class InputIterator, class OutputIterator>
    InputIterator decode(InputIterator in, OutputIterator out, int n) const
    {
        static_assert(sizeof(value_type_of<InputIterator>) == byte_size);
        static_assert(sizeof(value_type_of<OutputIterator>) == int_size);
        auto inptr = reinterpret_cast<const std::uint8_t*>(&*in);
        auto outptr = reinterpret_cast<std::uint32_t*>(&*out);
        std::size_t size = streamvbyte_decode(inptr, outptr, n);
        return std::next(in, size);
    }

    template<class InputIterator, class OutputIterator>
    InputIterator delta_decode(
        InputIterator in, OutputIterator out, int n, T initial = T()) const
    {
        static_assert(sizeof(value_type_of<InputIterator>) == byte_size);
        static_assert(sizeof(value_type_of<OutputIterator>) == int_size);
        auto inptr = reinterpret_cast<const std::uint8_t*>(&*in);
        auto outptr = reinterpret_cast<std::uint32_t*>(&*out);
        std::size_t size = streamvbyte_delta_decode(
            inptr, outptr, n, static_cast<std::uint32_t>(initial));
        return std::next(in, size);
    }
};

};  // namespace irk
