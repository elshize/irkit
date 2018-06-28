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

#include <string>

#include <type_safe/strong_typedef.hpp>
#include <type_safe/index.hpp>

namespace irk::index {

namespace ts = type_safe;

// TODO: Change aliases to strong types.
using term_id_t = type_safe::index_t;
using term_t = std::string;
using offset_t = std::size_t;

struct document_t : ts::strong_typedef<document_t, long>,
                    ts::strong_typedef_op::equality_comparison<document_t>,
                    ts::strong_typedef_op::relational_comparison<document_t>,
                    ts::strong_typedef_op::unary_plus<document_t>,
                    ts::strong_typedef_op::unary_minus<document_t>,
                    ts::strong_typedef_op::addition<document_t>,
                    ts::strong_typedef_op::subtraction<document_t> {
    using strong_typedef::strong_typedef;

    constexpr document_t() noexcept : strong_typedef(0) {}

    template<typename T,
        typename = typename std::enable_if<ts::detail::
                is_safe_integer_conversion<T, std::ptrdiff_t>::value>::type>
    constexpr document_t(T d) noexcept : strong_typedef(d)
    {}

    operator long() const noexcept { return ts::get(*this); }
};

std::ostream& operator<<(std::ostream& out, const document_t& d)
{
    return out << ts::get(d);
}

document_t operator"" _id(unsigned long long n) { return 0; }

//struct offset_t : ts::strong_typedef<offset_t, ptrdiff_t>,
//                  ts::strong_typedef_op::equality_comparison<offset_t>,
//                  ts::strong_typedef_op::relational_comparison<offset_t>,
//                  ts::strong_typedef_op::unary_plus<offset_t>,
//                  ts::strong_typedef_op::unary_minus<offset_t>,
//                  ts::strong_typedef_op::addition<offset_t>,
//                  ts::strong_typedef_op::subtraction<offset_t>,
//                  ts::strong_typedef_op::bitshift<offset_t, ptrdiff_t> {
//    using strong_typedef::strong_typedef;
//
//    constexpr offset_t() noexcept : strong_typedef(0) {}
//
//    template<typename T,
//        typename = typename std::enable_if<ts::detail::
//                is_safe_integer_conversion<T, std::ptrdiff_t>::value>::type>
//    constexpr offset_t(T o) noexcept : strong_typedef(o)
//    {}
//
//    explicit operator ptrdiff_t() const noexcept { return ts::get(*this); }
//};
//
//std::ostream& operator<<(std::ostream& out, const offset_t& d)
//{
//    return out << ts::get(d);
//}

};  // namespace irk::index
