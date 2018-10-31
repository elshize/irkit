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
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

#include <fmt/format.h>

#include <irkit/assert.hpp>

namespace irk {

struct RealRange {
    constexpr RealRange() : left(0.0), right(std::numeric_limits<double>::max())
    {}
    explicit constexpr RealRange(double left, double right)
        : left(left), right(right)
    {}
    double left;
    double right;
};

struct IntegralRange {
    constexpr IntegralRange()
        : left(0), right(std::numeric_limits<std::int64_t>::max())
    {}
    explicit constexpr IntegralRange(int64_t left, int64_t right)
        : left(left), right(right)
    {}
    int64_t left;
    int64_t right;
};

class LinearQuantizer {
public:
    constexpr LinearQuantizer(
        RealRange real_range, IntegralRange integral_range)
        : real_shift_(real_range.left),
          real_length_(real_range.right - real_shift_),
          integral_shift_(integral_range.left),
          integral_length_(integral_range.right - integral_shift_),
          ratio_(static_cast<double>(integral_length_) / real_length_)
    {
        assert(integral_shift_ >= 0);
    }

    std::int64_t operator()(double value)
    {
        return static_cast<int64_t>(ratio_ * (value - real_shift_))
            + integral_shift_;
    }

protected:
    const double real_shift_;
    const double real_length_;
    const std::int64_t integral_shift_;
    const std::int64_t integral_length_;
    const double ratio_;
};

}
