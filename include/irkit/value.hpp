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

#include <iostream>
#include <optional>

#include <nonstd/expected.hpp>

namespace irtl {

template<class T>
inline T value(std::optional<T>&& opt, std::string_view msg = {})
{
    if (not opt) {
        std::clog << "bad optional access";
        if (not msg.empty()) {
            std::clog << ": " << msg;
        }
        std::clog << '\n';
        std::abort();
    }
    return std::move(opt.value());
}

template<class T>
inline T value(const std::optional<T>& opt, std::string_view msg = {})
{
    if (not opt) {
        std::clog << "bad optional access";
        if (not msg.empty()) {
            std::clog << ": " << msg;
        }
        std::clog << '\n';
        std::abort();
    }
    return std::move(opt.value());
}

template<class T, class E>
inline T value(nonstd::expected<T, E>&& exp, std::string_view msg = {})
{
    if (not exp) {
        std::clog << "bad expected access: ";
        if (not msg.empty()) {
            std::clog << msg << " - ";
        }
        std::clog << exp.error() << '\n';
        std::abort();
    }
    return std::move(exp.value());
}

template<class T, class E>
inline T value(const nonstd::expected<T, E>& exp, std::string_view msg = {})
{
    if (not exp) {
        std::clog << "bad expected access: ";
        if (not msg.empty()) {
            std::clog << msg << " - ";
        }
        std::clog << exp.error() << '\n';
        std::abort();
    }
    return exp.value();
}

}  // namespace irtl
