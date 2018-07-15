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

//!
//! \file
//! \author      Michal Siedlaczek
//! \copyright   MIT License

#pragma once

#include <iostream>
#include <iterator>

#include <irkit/types.hpp>

namespace irk {

template<class Codec, class InputIterator>
std::vector<char> encode(const Codec& codec, InputIterator lo, InputIterator hi)
{
    std::vector<char> data(codec.max_encoded_size(std::distance(lo, hi)));
    auto size = codec.encode(lo, hi, std::begin(data));
    data.resize(size);
    return data;
}

template<class Codec, class InputRange>
std::vector<char> encode(const Codec& codec, InputRange in)
{
    return encode(codec, std::begin(in), std::end(in));
}

template<class Codec, class T>
std::vector<char> encode(const Codec& codec, std::initializer_list<T> in)
{
    return encode(codec, std::begin(in), std::end(in));
}

template<class Codec, class InputIterator>
std::vector<char>
delta_encode(const Codec& codec, InputIterator lo, InputIterator hi)
{
    std::vector<char> data(codec.max_encoded_size(std::distance(lo, hi)));
    auto size = codec.delta_encode(lo, hi, std::begin(data));
    data.resize(size);
    return data;
}

template<class Codec, class InputIterator, class T>
std::vector<char>
delta_encode(const Codec& codec, InputIterator lo, InputIterator hi, T initial)
{
    std::vector<char> data(codec.max_encoded_size(std::distance(lo, hi)));
    auto size = codec.delta_encode(lo, hi, std::begin(data), initial);
    data.resize(size);
    return data;
}

template<class Codec, class InputRange>
std::vector<char> delta_encode(const Codec& codec, InputRange in)
{
    return delta_encode(codec, std::begin(in), std::end(in));
}

template<class Codec, class InputIterator>
std::vector<typename Codec::value_type>
decode(const Codec& codec, InputIterator in, int count)
{
    std::vector<typename Codec::value_type> data(count);
    codec.decode(in, std::begin(data), count);
    return data;
}

template<class Codec, class InputIterator>
std::vector<typename Codec::value_type> delta_decode(const Codec& codec,
    InputIterator in,
    int count,
    typename Codec::value_type initial = typename Codec::value_type())
{
    std::vector<typename Codec::value_type> data(count);
    codec.delta_decode(in, std::begin(data), count, initial);
    return data;
}

};  // namespace irk
