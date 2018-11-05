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

#include <chrono>

namespace irk {

/// Runs `fn` and returns time of its execution.
///
/// # Examples
/// ```
/// auto slept_for = irk::run_with_timer<std::chrono::seconds>([]() {});
/// ```
template<class Unit, class Function>
Unit run_with_timer(Function fn)
{
    auto start_time = std::chrono::steady_clock::now();
    fn();
    auto end_time = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<Unit>(end_time - start_time);
}

template<class Unit, class Function, class Handler>
auto run_with_timer(Function fn, Handler handler) -> decltype(fn())
{
    auto start_time = std::chrono::steady_clock::now();
    auto result = fn();
    auto end_time = std::chrono::steady_clock::now();
    handler(std::chrono::duration_cast<Unit>(end_time - start_time));
    return result;
}

}  // namespace irk
