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

#include <type_safe/index.hpp>
#include <type_safe/strong_typedef.hpp>

namespace irk::index {

    namespace details {
        using document_base_type = std::int32_t;
        using term_id_base_type = std::int32_t;
        using term_base_type = std::string;
        using frequency_base_type = std::int32_t;
    };

namespace ts = type_safe;

using term_id_t = details::term_id_base_type;
using term_t = details::term_base_type;
using offset_t = std::size_t;
using frequency_t = details::frequency_base_type;
using document_t = details::document_base_type;

// struct document_t : ts::strong_typedef<document_t,
// details::document_base_type>,
//                    ts::strong_typedef_op::equality_comparison<document_t>,
//                    ts::strong_typedef_op::relational_comparison<document_t>,
//                    ts::strong_typedef_op::unary_plus<document_t>,
//                    ts::strong_typedef_op::unary_minus<document_t>,
//                    ts::strong_typedef_op::increment<document_t>,
//                    ts::strong_typedef_op::addition<document_t>,
//                    ts::strong_typedef_op::subtraction<document_t>,
//                    ts::strong_typedef_op::mixed_addition<document_t,
//                        details::document_base_type>,
//                    ts::strong_typedef_op::mixed_subtraction<document_t,
//                        details::document_base_type>
//{
//    using strong_typedef::strong_typedef;
//
//    explicit constexpr document_t() noexcept : strong_typedef(0) {}
//
//    template<typename T,
//        typename =
//            typename std::enable_if<ts::detail::is_safe_integer_conversion<T,
//                details::document_base_type>::value>::type>
//    constexpr document_t(T d) noexcept : strong_typedef(d)
//    {}
//
//    explicit operator long() const noexcept { return ts::get(*this); }
//    explicit operator int() const noexcept { return ts::get(*this); }
//    explicit operator std::uint32_t() const noexcept { return ts::get(*this);
//    } explicit operator std::size_t() const noexcept { return ts::get(*this);
//    }
//};
//
// std::ostream& operator<<(std::ostream& out, const document_t& d)
//{
//    return out << ts::get(d);
//}

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

}  // namespace irk::index

//namespace std {
//
//    template<>
//    struct make_unsigned<irk::index::document_t> {
//        using type = irk::index::details::document_base_type;
//    };
//
//    template<>
//    struct numeric_limits<irk::index::document_t> {
//        static const bool is_specialized = true;
//        static irk::index::document_t max()
//        {
//            return numeric_limits<irk::index::details::document_base_type>()
//                .max();
//        }
//    };
//
//}

namespace irk::literals {

inline irk::index::document_t operator"" _id(unsigned long long n)  // NOLINT
{
    return irk::index::document_t(n);
}

}  // namespace irk::literal
