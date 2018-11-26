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

namespace irk {

/// A (potentially) type-safe vector.
///
/// It derives from `std::vector<V>` and works essentially like one.
/// The difference is that you define a key type as well, the outcome
/// being that if you use a strong type key, you can differentiate between
/// `vmap<IndexType_1, V>` and `vmap<IndexType_2, V>`.
template<typename K, typename V = K>
class vmap : protected std::vector<V> {
public:
    using value_type = V;
    using reference = V&;
    using const_reference = const V&;
    using size_type = typename std::vector<V>::size_type;
    using typename std::vector<V>::iterator;
    using typename std::vector<V>::const_iterator;

    using std::vector<V>::push_back;
    using std::vector<V>::emplace_back;
    using std::vector<V>::size;
    using std::vector<V>::empty;
    using std::vector<V>::begin;
    using std::vector<V>::cbegin;
    using std::vector<V>::end;
    using std::vector<V>::cend;
    using std::vector<V>::data;

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
