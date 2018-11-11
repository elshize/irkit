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

#include <algorithm>

namespace irk {

/// A shorthand for `std::transform` that writes to the same iterator.
template<class InputIt, class UnaryOperation>
constexpr UnaryOperation
inplace_transform(InputIt first, InputIt last, UnaryOperation unary_op)
{
    std::transform(first, last, first, unary_op);
    return std::move(unary_op);
}

/// A shorthand for `std::transform` that writes to the same iterator.
template<class ExecutionPolicy, class ForwardIt, class UnaryOperation>
UnaryOperation inplace_transform(
    ExecutionPolicy&& policy,
    ForwardIt first,
    ForwardIt last,
    UnaryOperation unary_op)
{
    std::transform(policy, first, last, first, unary_op);
    return std::move(unary_op);
}

/// A shorthand for `std::transform` that takes an entire range.
template<class InputRange, class OutputIt, class UnaryOperation>
constexpr OutputIt transform_range(
    const InputRange& input_range, OutputIt d_first, UnaryOperation unary_op)
{
    return std::transform(
        std::begin(input_range), std::end(input_range), d_first, unary_op);
}

/// A shorthand for `std::transform` that takes an entire range.
template<
    class ExecutionPolicy,
    class InputRange,
    class OutputIt,
    class UnaryOperation>
constexpr OutputIt transform_range(
    ExecutionPolicy&& policy,
    const InputRange& input_range,
    OutputIt d_first,
    UnaryOperation unary_op)
{
    return std::transform(
        policy,
        std::begin(input_range),
        std::end(input_range),
        d_first,
        unary_op);
}

/// A shorthand for `std::transform` that takes an entire range.
template<
    class InputRange1,
    class InputRange2,
    class OutputIt,
    class BinaryOperation>
constexpr OutputIt transform_ranges(
    const InputRange1& input_range1,
    const InputRange2& input_range2,
    OutputIt d_first,
    BinaryOperation binary_op)
{
    return std::transform(
        std::begin(input_range1),
        std::end(input_range1),
        std::begin(input_range2),
        d_first,
        binary_op);
}

/// A shorthand for `std::transform` that takes an entire range.
template<
    class ExecutionPolicy,
    class InputRange1,
    class InputRange2,
    class OutputIt,
    class BinaryOperation>
constexpr OutputIt transform_ranges(
    ExecutionPolicy&& policy,
    const InputRange1& input_range1,
    const InputRange2& input_range2,
    OutputIt d_first,
    BinaryOperation binary_op)
{
    return std::transform(
        policy,
        std::begin(input_range1),
        std::end(input_range1),
        std::begin(input_range2),
        std::end(input_range2),
        d_first,
        binary_op);
}

/// A shorthand for `std::transform` that takes an entire range and transforms
/// in place.
template<class InputRange, class UnaryOperation>
constexpr UnaryOperation
inplace_transform_range(const InputRange& input_range, UnaryOperation unary_op)
{
    return inplace_transform(
        std::begin(input_range), std::end(input_range), unary_op);
}

/// A shorthand for `std::transform` that takes an entire range and transforms
/// in place.
template<class ExecutionPolicy, class InputRange, class UnaryOperation>
constexpr UnaryOperation inplace_transform_range(
    ExecutionPolicy&& policy,
    const InputRange& input_range,
    UnaryOperation unary_op)
{
    return inplace_transform(
        policy, std::begin(input_range), std::end(input_range), unary_op);
}

}  // namespace irk
