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

#include <debug_assert.hpp>

#define ASSERT(Expr)                                                           \
    DEBUG_ASSERT(Expr, assert_handler{}, debug_assert::level<1>{})

#define EXPECTS(Expr)                                                          \
    DEBUG_ASSERT(                                                              \
        Expr,                                                                  \
        contract_handler{},                                                    \
        debug_assert::level<1>{},                                              \
        "function input contract violated")

#define ENSURES(Expr)                                                          \
    DEBUG_ASSERT(                                                              \
        Expr,                                                                  \
        contract_handler{},                                                    \
        debug_assert::level<1>{},                                              \
        "function output contract violated")

namespace irk {

struct assert_handler : debug_assert::default_handler,
                        debug_assert::set_level<1> {};

struct contract_handler : debug_assert::default_handler,
                          debug_assert::set_level<1> {};

struct debug_handler : debug_assert::default_handler,
                       debug_assert::set_level<
#ifdef DEBUG_ASSERT
                           1
#else
                           0
#endif
                           > {
        };

#ifdef DEBUG_ASSERT
        constexpr bool Debug = true;
#else
        constexpr bool Debug = false;
#endif

}  // namespace irk
