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

#include <irkit/iterator/block_iterator.hpp>

namespace ir {

/// This class is made for testing, to emulate a real block list.
template<class V>
class Vector_Block_List {
public:
    using size_type      = std::int32_t;
    using value_type     = V;
    using iterator       = Block_Iterator<Vector_Block_List<V>>;
    using const_iterator = iterator;

    constexpr Vector_Block_List() = default;
    constexpr Vector_Block_List(irk::index::term_id_t term_id,
                                std::vector<value_type> vec)
        : term_id_{term_id},
          ids_{std::move(vec)},
          block_size_{ids_.size()},
          bounds_{set_up_bounds()}
    {}
    constexpr Vector_Block_List(irk::index::term_id_t term_id,
                                std::vector<value_type> vec,
                                size_type block_size)
        : term_id_{term_id},
          ids_{std::move(vec)},
          block_size_{block_size},
          bounds_{set_up_bounds()}
    {}
    constexpr Vector_Block_List(const Vector_Block_List&)     = default;
    constexpr Vector_Block_List(Vector_Block_List&&) noexcept = default;
    constexpr Vector_Block_List& operator=(const Vector_Block_List&) = default;
    constexpr Vector_Block_List& operator=(Vector_Block_List&&) noexcept = default;

    [[nodiscard]] constexpr auto size() const -> size_type { return ids_.size(); }
    [[nodiscard]] constexpr auto block_size() const -> size_type { return block_size_; }
    [[nodiscard]] constexpr auto block_count() const noexcept -> size_type
    {
        return (size() + block_size_ - 1) / block_size_;
    }

    [[nodiscard]] constexpr auto operator[](size_type pos) const -> value_type const&
    {
        return ids_[pos];
    }

    [[nodiscard]] constexpr auto begin() const noexcept -> iterator
    {
        return iterator({0, 0}, *this);
    }
    [[nodiscard]] constexpr auto end() const noexcept -> iterator
    {
        size_type block = ids_.size() / block_size_;
        size_type pos = ids_.size() % block_size_;
        return iterator{{block, pos}, *this};
    }
    [[nodiscard]] constexpr auto lookup(value_type id) const -> iterator
    {
        return begin().nextgeq(id);
    }
    [[nodiscard]] constexpr auto term_id() const { return term_id_; }

    [[nodiscard]] constexpr auto
    block(std::ptrdiff_t n) const noexcept -> gsl::span<value_type const>
    {
        auto begin = ids_.data() + n * block_size_;
        auto len = n == irk::sgnd(ids_.size()) / block_size_
            ? ids_.size() % block_size_
            : block_size_;
        return gsl::make_span<value_type const>(begin, len);
    }

    [[nodiscard]] constexpr auto upper_bounds() const { return bounds_; }

private:
    [[nodiscard]] constexpr auto set_up_bounds() {
        auto count = block_count();
        std::vector<value_type> bounds(count);
        for (auto block : iter::range(count)) {
            auto bound_idx = (block + 1) * block_size_ - 1;
            if (bound_idx < size()) {
                bounds[block] = ids_[bound_idx];
            } else {
                bounds[block] = ids_.back();
            }
        }
        return bounds;
    }

    irk::index::term_id_t term_id_{};
    std::vector<value_type> ids_{};
    size_type block_size_{};
    std::vector<value_type> bounds_{};
};

}  // namespace ir
