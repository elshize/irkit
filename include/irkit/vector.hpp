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

#include <vector>

#include <cppitertools/itertools.hpp>
#include <gsl/span>

namespace ir {

/// A (potentially) type-safe vector.
///
/// It derives from `std::vector<V>` and works essentially like one.
/// The difference is that you define a key type as well, the outcome
/// being that if you use a strong type key, you can differentiate between
/// `Vector<IndexType_1, V>` and `Vector<IndexType_2, V>`.
template<typename K, typename V = K, typename Allocator = std::allocator<V>>
class Vector : protected std::vector<V> {
public:
    using typename std::vector<V, Allocator>::value_type;
    using typename std::vector<V, Allocator>::reference;
    using typename std::vector<V, Allocator>::const_reference;
    using typename std::vector<V, Allocator>::size_type;
    using typename std::vector<V, Allocator>::iterator;
    using typename std::vector<V, Allocator>::const_iterator;

    using std::vector<V, Allocator>::assign;
    using std::vector<V, Allocator>::get_allocator;

    using std::vector<V, Allocator>::at;
    using std::vector<V, Allocator>::front;
    using std::vector<V, Allocator>::back;
    using std::vector<V, Allocator>::data;

    using std::vector<V, Allocator>::begin;
    using std::vector<V, Allocator>::cbegin;
    using std::vector<V, Allocator>::end;
    using std::vector<V, Allocator>::cend;
    using std::vector<V, Allocator>::rbegin;
    using std::vector<V, Allocator>::crbegin;
    using std::vector<V, Allocator>::rend;
    using std::vector<V, Allocator>::crend;

    using std::vector<V, Allocator>::empty;
    using std::vector<V, Allocator>::size;
    using std::vector<V, Allocator>::max_size;
    using std::vector<V, Allocator>::reserve;
    using std::vector<V, Allocator>::capacity;
    using std::vector<V, Allocator>::shrink_to_fit;

    using std::vector<V, Allocator>::clear;
    using std::vector<V, Allocator>::insert;
    using std::vector<V, Allocator>::emplace;
    using std::vector<V, Allocator>::erase;
    using std::vector<V, Allocator>::push_back;
    using std::vector<V, Allocator>::emplace_back;
    using std::vector<V, Allocator>::pop_back;
    using std::vector<V, Allocator>::resize;
    using std::vector<V, Allocator>::swap;

    Vector() noexcept(noexcept(Allocator())) : std::vector<V, Allocator>() {}
    explicit Vector(Allocator const& alloc) noexcept : std::vector<V, Allocator>(alloc) {}
    Vector(size_type count, V const& value, Allocator const& alloc = Allocator())
        : std::vector<V, Allocator>(count, value, alloc)
    {}
    explicit Vector(size_type count, Allocator const& alloc = Allocator())
        : std::vector<V, Allocator>(count, alloc)
    {}
    template<class InputIt>
    Vector(InputIt first, InputIt last, Allocator const& alloc = Allocator())
        : std::vector<V, Allocator>(first, last, alloc)
    {}
    Vector(Vector const& other) : std::vector<V, Allocator>(other) {}
    Vector(Vector const& other, const Allocator& alloc) : std::vector<V, Allocator>(other, alloc) {}
    Vector(Vector&& other) noexcept : std::vector<V, Allocator>(other) {}
    Vector(Vector&& other, Allocator const& alloc) : std::vector<V, Allocator>(other, alloc) {}
    Vector(std::initializer_list<V> init, Allocator const& alloc = Allocator())
        : std::vector<V, Allocator>(init, alloc)
    {}
    ~Vector() = default;

    Vector& operator=(Vector const& other)
    {
        std::vector<V, Allocator>::operator=(other);
        return *this;
    };
    Vector& operator=(Vector&& other) noexcept
    {
        std::vector<V, Allocator>::operator=(other);
        return *this;
    };
    Vector& operator=(std::initializer_list<V> init)
    {
        std::vector<V, Allocator>::operator=(init);
        return *this;
    }

    reference operator[](K key)
    {
        return std::vector<V, Allocator>::operator[](static_cast<size_type>(key));
    }
    const_reference operator[](K key) const
    {
        return std::vector<V, Allocator>::operator[](static_cast<size_type>(key));
    }
    reference at(K key)
    {
        return std::vector<V, Allocator>::at(static_cast<size_type>(key));
    }
    const_reference at(K key) const
    {
        return std::vector<V, Allocator>::at(static_cast<size_type>(key));
    }

    std::vector<V> const& as_vector() const { return *this; }

    auto entries()
    {
        return iter::zip(
            iter::imap([](auto const& idx) { return static_cast<K>(idx); }, iter::range(size())),
            *this);
    }
    auto entries() const
    {
        return iter::zip(
            iter::imap([](auto const& idx) { return static_cast<K>(idx); }, iter::range(size())),
            *this);
    }
};

template<class K, class V, class Alloc>
void swap(Vector<K, V, Alloc>& lhs, Vector<K, V, Alloc>& rhs) noexcept(noexcept(lhs.swap(rhs)))
{
    return lhs.swap(rhs);
}

template<class K, class V, class Alloc>
bool operator==(const Vector<K, V, Alloc>& lhs, const Vector<K, V, Alloc>& rhs)
{
    return lhs.as_vector() == rhs.as_vector();
}
template<class K, class V, class Alloc>
bool operator!=(const Vector<K, V, Alloc>& lhs, const Vector<K, V, Alloc>& rhs)
{
    return lhs.as_vector() != rhs.as_vector();
}
template<class K, class V, class Alloc>
bool operator<(const Vector<K, V, Alloc>& lhs, const Vector<K, V, Alloc>& rhs)
{
    return lhs.as_vector() < rhs.as_vector();
}
template<class K, class V, class Alloc>
bool operator<=(const Vector<K, V, Alloc>& lhs, const Vector<K, V, Alloc>& rhs)
{
    return lhs.as_vector() <= rhs.as_vector();
}
template<class K, class V, class Alloc>
bool operator>(const Vector<K, V, Alloc>& lhs, const Vector<K, V, Alloc>& rhs)
{
    return lhs.as_vector() > rhs.as_vector();
}
template<class K, class V, class Alloc>
bool operator>=(const Vector<K, V, Alloc>& lhs, const Vector<K, V, Alloc>& rhs)
{
    return lhs.as_vector() >= rhs.as_vector();
}

template<typename K, typename V = K>
class Vector_View : protected gsl::span<std::add_const_t<V>> {
public:
    using element_type = std::add_const_t<V>;
    using gsl::span<element_type>::value_type;
    using gsl::span<element_type>::index_type;
    using gsl::span<element_type>::pointer;
    using gsl::span<element_type>::reference;

    using gsl::span<element_type>::iterator;
    using gsl::span<element_type>::const_iterator;
    using gsl::span<element_type>::reverse_iterator;
    using gsl::span<element_type>::const_reverse_iterator;

    using size_type = std::int64_t;

    constexpr Vector_View() noexcept = default;
    template<typename M>
    explicit Vector_View(M&& memory_view)
        : gsl::span<element_type>(reinterpret_cast<V const*>(memory_view.data()),
                                  memory_view.size() / sizeof(V))
    {}
    ~Vector_View() noexcept = default;
    using gsl::span<element_type>::size;
    using gsl::span<element_type>::size_bytes;
    using gsl::span<element_type>::empty;

    using gsl::span<element_type>::data;
    using gsl::span<element_type>::begin;
    using gsl::span<element_type>::end;
    using gsl::span<element_type>::cbegin;
    using gsl::span<element_type>::cend;
    using gsl::span<element_type>::rbegin;
    using gsl::span<element_type>::rend;
    using gsl::span<element_type>::crbegin;
    using gsl::span<element_type>::crend;

    [[nodiscard]] constexpr auto
    operator[](K key) -> typename gsl::span<element_type>::reference const
    {
        return data()[key];
    }
    [[nodiscard]] constexpr auto at(K key) const -> typename gsl::span<element_type>::reference
    {
        return gsl::span<element_type>::operator[](key);
    }

    template<typename M>
    [[nodiscard]] constexpr static Vector_View load(M memory_view)
    {
        return Vector_View(
            gsl::span<element_type>(memory_view.data() + sizeof(gsl::span<element_type>::size_type),
                                    compute_size(memory_view)));
    }

private:
    template<typename M>
    [[nodiscard]] constexpr static auto compute_size(M const& memory)
    {
        using usize = std::make_unsigned_t<size_type>;
        usize nbytes = *reinterpret_cast<usize const*>(memory.data());
        nbytes = (nbytes << 8) >> 8;
        return nbytes / sizeof(V);
    }
};

}  // namespace ir
