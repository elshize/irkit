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

//! \file bitstream.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <iostream>

namespace irk {

//! An input stream reading bits.
class input_bit_stream {
protected:
    std::istream& in_;
    char byte_;
    std::uint8_t buffered_pos_;

    std::int8_t get_bit(int n) { return ((byte_ & (1 << n)) != 0); }

public:
    explicit input_bit_stream(std::istream& in) : in_(in), buffered_pos_(8) {}

    //! Returns bit: 0 or 1, or -1 if no bit could be read.
    std::int8_t read()
    {
        if (buffered_pos_ == 8)
        {
            if (!in_.read(&byte_, 1)) { return -1; }
            buffered_pos_ = 0;
        }
        return get_bit(buffered_pos_++);
    };

    void clear_buffer() { buffered_pos_ = 8; }
};

//! An output stream writing bits.
class output_bit_stream {
protected:
    std::ostream& out_;
    char byte_;
    std::uint8_t buffered_bits_;

    void set_bit(int n, bool bit) { byte_ |= (bit << n); }

    void do_flush()
    {
        out_.write(&byte_, 1);
        buffered_bits_ = 0;
        byte_ = 0;
    }

public:
    explicit output_bit_stream(std::ostream& out)
        : out_(out), byte_(0), buffered_bits_(0)
    {}

    void write(bool bit)
    {
        set_bit(buffered_bits_++, bit);
        if (buffered_bits_ == 8) { do_flush(); }
    };

    void flush()
    {
        if (buffered_bits_ > 0) { do_flush(); }
        out_.flush();
    }
};

};  // namespace irk
