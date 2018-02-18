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

//! \file movingrange.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <utility>

namespace irk {

//! A container for two ends of iterators.
/*!
    \author Michal Siedlaczek <http://github.com/elshize>

    TODO:
        - Needs to be generalized, for containers as well?
        - Implement iterator methods.
 */
template<class Iter>
struct moving_range {
    using iterator_type = Iter;
    using element_type = decltype(*std::declval<Iter>());

    //! The left end of the range.
    iterator_type left;

    //! The right end of the range (after the last element).
    iterator_type right;

    moving_range() = default;
    moving_range(iterator_type first, iterator_type last)
        : left(first), right(last)
    {}

    bool empty() const { return left == right; }

    //! Advances the left end of the range by one.
    void advance() { ++left; };

    //! Advances the left end of the range by `n`.
    void advance(unsigned int n) { left += n; };

    iterator_type begin() const { return left; }
    iterator_type end() const { return right; }
    auto front() const { return *left; }
};

};  // namespace irk
