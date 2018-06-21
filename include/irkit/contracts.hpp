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

#include <stdexcept>

namespace irk::runtime {

struct leq_type {
    template<class L, class R>
    bool operator()(const L& left, const R& right)
    { return left <= right; }
} LEQ;

struct lt_type {
    template<class L, class R>
    bool operator()(const L& left, const R& right)
    { return left < right; }
} LT;

struct eq_type {
    template<class L, class R>
    bool operator()(const L& left, const R& right)
    { return left == right; }
} EQ;

struct neq_type {
    template<class L, class R>
    bool operator()(const L& left, const R& right)
    { return left != right; }
} NEQ;

template<class L, class R, class AssertFn>
void expects(const L& left, AssertFn fn, const R& right) {
    if (!fn(left, right)) { throw std::runtime_error(fn.print_message()); }
}

};  // namespace irk::runtime
