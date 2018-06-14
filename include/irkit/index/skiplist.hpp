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

//! \file skiplist.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <gsl/span>
#include <irkit/compacttable.hpp>
#include <irkit/memoryview.hpp>

namespace irk::index
{

using byte_iterator = typename irk::memory_view::iterator;

template<class Skip,
    int element_size = sizeof(Skip),
    class CastFn = irk::reinterpret_cast_fn<Skip>>
class skip_list_view {
public:
    using skip_type = Skip;

    struct const_iterator {
        using self_type = const_iterator;
        using value_type = skip_type;
        using reference = skip_type&;
        using pointer = skip_type*;
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = std::ptrdiff_t;

        byte_iterator pos;
        CastFn cast_fn_;

        constexpr skip_type operator*() const
        {
            return cast_fn_(&(*pos));
        }

        constexpr const_iterator& operator++()
        {
            pos += element_size;
            return *this;
        }

        constexpr const_iterator operator++(int)
        {
            auto ret = *this;
            ++(*this);
            return ret;
        }

        constexpr const_iterator& operator--()
        {
            pos -= element_size;
            return *this;
        }

        constexpr const_iterator operator--(int)
        {
            auto ret = *this;
            --(*this);
            return ret;
        }

        constexpr const_iterator operator+(difference_type n) const
        {
            return const_iterator{pos + (n * element_size)};
        }

        constexpr const_iterator& operator+=(difference_type n)
        {
            pos += n * element_size;
            return *this;
        }

        constexpr const_iterator operator-(difference_type n) const
        {
            return const_iterator{pos - (n * element_size)};
        }

        constexpr const_iterator& operator-=(difference_type n)
        {
            pos -= n * element_size;
            return *this;
        }

        constexpr friend bool
        operator==(const_iterator lhs, const_iterator rhs) noexcept
        {
            return lhs.pos == rhs.pos && lhs.pos == rhs.pos;
        }

        constexpr friend bool
        operator!=(const_iterator lhs, const_iterator rhs) noexcept
        {
            return !(lhs == rhs);
        }
    };
    using iterator = const_iterator;

private:
    irk::memory_view skip_view_;
    gsl::index block_count_;

    irk::memory_view create_skip_view(const irk::memory_view& view)
    {
        int count = view.range(0, sizeof(int)).as<int>();
        std::cout << count << std::endl;
        return view.range(sizeof(count), count * element_size);
    }

public:
    skip_list_view() = default;
    skip_list_view(const skip_list_view&) = default;
    skip_list_view(skip_list_view&&) = default;
    skip_list_view(irk::memory_view memory_view)
        : skip_view_(create_skip_view(memory_view)),
          block_count_(skip_view_.size() / element_size)
    {}

    auto block_count() const noexcept { return block_count_; }
    auto size() const noexcept { return skip_view_.size(); }

    constexpr const_iterator begin() const noexcept { return cbegin(); }
    constexpr const_iterator cbegin() const noexcept
    {
        return const_iterator{skip_view_.begin()};
    }
    constexpr const_iterator end() const noexcept { return cend(); }
    constexpr const_iterator cend() const noexcept
    {
        return const_iterator{skip_view_.end()};
    }
};

template<class Doc, class Skip>
struct idskip {
    Doc doc;
    Skip skip;
};

template<class Doc, class Skip>
bool operator==(const idskip<Doc, Skip>& lhs, const idskip<Doc, Skip>& rhs)
{
    return lhs.doc == rhs.doc && lhs.skip == rhs.skip;
}

template<class Doc, class Skip>
struct idskip_cast_fn {
    static constexpr int doc_size = sizeof(Doc);
    idskip<Doc, Skip> operator()(const char* pos) const
    {
        return {*reinterpret_cast<Doc*>(pos),
            *reinterpret_cast<Skip*>(pos + doc_size)};
    }
};

template<class Doc, class Skip>
using id_skip_list_view = skip_list_view<idskip<Doc, Skip>,
    sizeof(Doc) + sizeof(Skip),
    idskip_cast_fn<Doc, Skip>>;


template<class T>
struct write_fn {
    std::ostream& operator()(std::ostream& out, T value) const
    {
        out.write(value);
        return out;
    }
};

//! Builds and writes skip lists for postings.
template<class Skip,
    int element_size = sizeof(Skip),
    class WriteFn = write_fn<Skip>>
class skip_list_builder {
private:
    std::ostream& skips_out_;
    std::ostream& offsets_out_;
    const int block_size_;
    std::vector<std::size_t> offsets_;
    WriteFn write_;

public:
    skip_list_builder(std::ostream& skips_out,
        std::ostream& offsets_out,
        int block_size) noexcept
        : skips_out_(skips_out),
          offsets_out_(offsets_out),
          block_size_(block_size)
    {}

    void write_term_skips()
    {
    }

    void close() const
    {
        irk::offset_table<> offset_table = build_offset_table<>(offsets_);
        offsets_out_ << offset_table;
    }

};

};  // namespace irk::index
