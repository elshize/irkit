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

#include <cstdint>
#include <vector>

#include <irkit/index/raw_inverted_list.hpp>

namespace ir {

struct Blocked_Position {
    std::int32_t block;
    std::int32_t offset;

    [[nodiscard]] bool operator==(Blocked_Position const& other) const
    {
        return (block == other.block) & (offset == other.offset);
    }

    [[nodiscard]] bool operator!=(Blocked_Position const& other) const
    {
        return (block != other.block) | (offset != other.offset);
    }
};

template<class List>
class Block_Iterator {
public:
    using value_type        = typename List::value_type;
    using difference_type   = std::ptrdiff_t;
    using size_type         = std::int32_t;
    using reference         = value_type const&;
    using pointer           = value_type const*;
    using iterator_category = std::forward_iterator_tag;

    Block_Iterator() noexcept = default;
    constexpr Block_Iterator(Blocked_Position position,
                            List const& list) noexcept
        : position_{position}, list_{&list}
    {}
    Block_Iterator(Block_Iterator const& position) noexcept = default;
    Block_Iterator(Block_Iterator&& position) noexcept = default;
    Block_Iterator&
    operator=(Block_Iterator const& position) noexcept = default;
    Block_Iterator& operator=(Block_Iterator&& position) noexcept = default;
    ~Block_Iterator() noexcept = default;

    [[nodiscard]] constexpr reference operator*() const
    {
        return list_->block(position_.block)[position_.offset];
    }

    [[nodiscard]] constexpr pointer operator->() const
    {
        return &list_->block(position_.block)[position_.offset];
    }

    constexpr Block_Iterator& operator++() noexcept
    {
        auto block_size = list_->block_size();
        ++position_.offset;
        position_.block += position_.offset / block_size;
        position_.offset %= block_size;
        return *this;
    }

    [[nodiscard]] constexpr Block_Iterator operator++(int) noexcept
    {
        auto copy = *this;
        this->operator++();
        return copy;
    }

    [[nodiscard]] constexpr bool
    operator==(Block_Iterator const& other) const noexcept
    {
        return position_ == other.position_;
    }

    [[nodiscard]] constexpr bool
    operator!=(Block_Iterator const& other) const noexcept
    {
        return position_ != other.position_;
    }

    constexpr Block_Iterator& advance_to(value_type val)
    {
        position_ = nextgeq_position(position_, val);
        if (position_.block >= list_->block_count()) {
            *this = end();
            return *this;
        } else {
            auto const& decoded_block = list_->block(position_.block);
            auto current = std::next(decoded_block.begin(), position_.offset);
            auto new_offset = std::lower_bound(
                current, decoded_block.end(), val);
            position_.offset += std::distance(current, new_offset);
        }
        return *this;
    };

    [[nodiscard]] constexpr auto next_ge(value_type val) const -> Block_Iterator
    {
        Block_Iterator<List> next{*this};
        next.advance_to(val);
        return next;
    };

    [[nodiscard]] constexpr auto idx() const noexcept -> size_type
    {
        return list_->block_size() * position_.block + position_.offset;
    }

    [[nodiscard]] constexpr auto blocked_postition() const noexcept -> Blocked_Position
    {
        return position_;
    }

    template<class Iterator>
    constexpr auto align(const Iterator& other) noexcept -> Block_Iterator&
    {
        position_ = other.blocked_postition();
        return *this;
    }

    [[nodiscard]] constexpr auto
    fetch(Block_Iterator until) const -> irk::raw_inverted_list<value_type>
    {
        return irk::raw_inverted_list<value_type>{
            list_->term_id(), *this, until};
    };

    [[nodiscard]] constexpr auto
    fetch() const -> irk::raw_inverted_list<value_type>
    {
        return fetch(end());
    }

    [[nodiscard]] constexpr static auto
    end(size_type length, size_type block_size, size_type block_count) -> Blocked_Position
    {
        return {length / block_size, (length - (block_count - 1) * block_size) % block_size};
    }

private:
    [[nodiscard]] auto
    nextgeq_position(Blocked_Position pos, value_type id) const
    {
        auto const& upper_bounds = list_->upper_bounds();
        auto current = std::next(std::begin(upper_bounds), pos.block);
        auto lower = std::lower_bound(
            current,
            std::end(upper_bounds),
            id,
            [](auto const& bound, auto const& id) { return bound < id; });
        pos.block += std::distance(current, lower);
        pos.offset = pos.block == position_.block ? pos.offset : 0;
        return pos;
    }

    constexpr void finish() noexcept
    {
        // auto len = list_->size();
        // auto block_size = list_->block_size();
        // position_.block = len / block_size;
        // position_.offset = (len - ((list_->block_count() - 1) * block_size))
        //    % block_size;
        position_ = end(list_->size(), list_->block_size(), list_->block_count());
    }

    [[nodiscard]] constexpr auto end() const noexcept -> Block_Iterator
    {
        Block_Iterator copy = *this;
        copy.finish();
        return copy;
    }

    Blocked_Position position_;
    List const* list_;
};

}  // namespace ir

namespace std {

template<class List>
struct iterator_traits<ir::Block_Iterator<List>> {
    using value_type = typename ir::Block_Iterator<List>::value_type;
    using difference_type = typename ir::Block_Iterator<List>::difference_type;
    using reference = typename ir::Block_Iterator<List>::reference;
    using pointer = typename ir::Block_Iterator<List>::pointer;
    using iterator_category = typename ir::Block_Iterator<
        List>::iterator_category;
};

}  // namespace std
