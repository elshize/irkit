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

#include <cppitertools/itertools.hpp>
#include <type_safe/index.hpp>
#include <type_safe/strong_typedef.hpp>

namespace irk {

namespace ts = type_safe;

namespace details {

    using shard_base_type = std::int32_t;

}  // namespace details

struct ShardId
    : ts::strong_typedef<ShardId, details::shard_base_type>,
      ts::strong_typedef_op::equality_comparison<ShardId, bool>,
      ts::strong_typedef_op::relational_comparison<ShardId, bool>,
      ts::strong_typedef_op::addition<ShardId>,
      ts::strong_typedef_op::subtraction<ShardId>,
      ts::strong_typedef_op::mixed_addition<ShardId, details::shard_base_type>,
      ts::strong_typedef_op::
          mixed_subtraction<ShardId, details::shard_base_type> {
    using strong_typedef::strong_typedef;
    explicit constexpr ShardId() noexcept : strong_typedef(0) {}
    template<
        typename T,
        typename =
            typename std::enable_if<ts::detail::is_safe_integer_conversion<
                details::shard_base_type,
                T>::value>::type>
    explicit constexpr ShardId(T d) noexcept : strong_typedef(d)
    {}
    explicit operator size_t() const noexcept { return ts::get(*this); }
    details::shard_base_type as_int() const noexcept { return ts::get(*this); }

    auto static range(details::shard_base_type count)
    {
        return iter::imap(
            [](auto shard) { return ShardId(shard); }, iter::range(count));
    }
};

std::ostream& operator<<(std::ostream& os, const ShardId& shard)
{
    return os << static_cast<details::shard_base_type>(shard);
}

template<typename K, typename V>
class vmap : protected std::vector<V> {
public:
    using reference = V&;
    using const_reference = const V&;
    using size_type = typename std::vector<V>::size_type;
    using std::vector<V>::push_back;
    using std::vector<V>::emplace_back;
    using std::vector<V>::size;
    using std::vector<V>::empty;
    using std::vector<V>::begin;
    using std::vector<V>::cbegin;
    using std::vector<V>::end;
    using std::vector<V>::cend;

    vmap() : std::vector<V>(){};
    ~vmap() = default;
    explicit vmap(size_type count) : std::vector<V>(count){};
    vmap(size_type count, const V& value) : std::vector<V>(count, value){};
    vmap(std::initializer_list<V> init) : std::vector<V>(init) {}
    vmap(const vmap& other) : std::vector<V>(other){};
    vmap(vmap&& other) noexcept : std::vector<V>(other){};

    template<class InputIt>
    vmap(InputIt first, InputIt last) : std::vector<V>(first, last)
    {}

    vmap& operator=(const vmap& other) {
        std::vector<V>::operator=(other);
        return *this;
    };
    vmap& operator=(vmap&& other) noexcept {
        std::vector<V>::operator=(other);
        return *this;
    };
    reference operator[](K id)
    {
        return std::vector<V>::operator[](static_cast<size_type>(id));
    }
    const_reference operator[](K id) const
    {
        return std::vector<V>::operator[](static_cast<size_type>(id));
    }
    const std::vector<V>& as_vector() const { return *this; }

    auto entries()
    {
        return iter::zip(
            iter::imap(
                [](const auto& idx) { return static_cast<K>(idx); },
                iter::range(size())),
            *this);
    }
    auto entries() const
    {
        return iter::zip(
            iter::imap(
                [](const auto& idx) { return static_cast<K>(idx); },
                iter::range(size())),
            *this);
    }
};

}  // namespace irk

namespace irk::index {

    namespace details {

        using document_base_type = std::int32_t;
        using term_id_base_type = std::int32_t;
        using term_base_type = std::string;
        using frequency_base_type = std::int32_t;
        using shard_base_type = std::int32_t;

    }  // namespace details

using term_id_t = details::term_id_base_type;
using term_t = details::term_base_type;
using offset_t = std::size_t;
using frequency_t = details::frequency_base_type;
using document_t = details::document_base_type;

}  // namespace irk::index

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

//}  // namespace irk::index

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

}  // namespace irk::literals
