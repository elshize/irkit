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

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>

#include <irkit/bitstream.hpp>
#include <irkit/types.hpp>

namespace irk {

template<class Codec>
class prefix_codec {
public:
    explicit prefix_codec(Codec codec) : codec_(std::move(codec)) {}
    prefix_codec(const prefix_codec&) = default;
    prefix_codec(prefix_codec&&) noexcept = default;
    prefix_codec& operator=(const prefix_codec&) = default;
    prefix_codec& operator=(prefix_codec&&) noexcept = delete;
    ~prefix_codec() = default;

    output_bit_stream&
    encode(const std::string& value, output_bit_stream& out) const
    {
        int pos = 0;
        for (; pos < prev_.size() && pos < value.size(); ++pos)
        { if (prev_[pos] != value[pos]) { break; } }

        encode_unary(pos, out);
        encode_unary(value.size() - pos, out);
        codec_.encode(std::next(value.cbegin(), pos), value.cend(), out);
        prev_ = value;
        return out;
    }

    std::streamsize
    decode(input_bit_stream& in, std::string& value) const
    {
        int prefix_length = decode_unary(in);
        int suffix_length = decode_unary(in);
        value = prev_.substr(0, prefix_length);
        boost::iostreams::stream<
            boost::iostreams::back_insert_device<std::string>>
            buffer(boost::iostreams::back_inserter(value));
        auto size = codec_.decode(in, buffer, suffix_length) + prefix_length
            + suffix_length + 2;
        buffer.flush();
        prev_ = value;
        return size;
    }

    void reset() const { prev_ = ""; }

    const Codec& codec() const { return codec_; }

private:
    Codec codec_;
    mutable std::string prev_{};

    void encode_unary(int n, output_bit_stream& out) const
    {
        for (int idx = 0; idx < n; ++idx) { out.write(true); }
        out.write(false);
    }

    int decode_unary(input_bit_stream& in) const
    {
        int value = 0;
        int bit;
        while ((bit = in.read()) > 0) { ++value; }
        return value;
    }
};

};
