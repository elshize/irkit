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

#include <optional>
#include <vector>

#include <gsl/span>

#include <irkit/coding.hpp>
#include <irkit/memoryview.hpp>

namespace irk::index {

//! A view to an encoded block in an inverted index.
//!
//! \tparam T   the type of values encoded in the block
//!
template<class T = std::nullopt_t>
class block_view {
    bool constexpr static supports_skips =
        not std::is_same_v<T, std::nullopt_t>;

public:
    using value_type = T;
    using char_type = char;

    //! Constructs a block view **with** its last value to support skips.
    //! \param last_value   last value of the block
    //! \param memory       underlying memory
    explicit block_view(T last_value, irk::memory_view memory)
        : memory_view_(std::move(memory))
    {
        static_assert(supports_skips,
            "must define parameter type to construct with a value");
        last_value_[0] = last_value;
    }

    //! Constructs a block view **without** its last value (no skips supported).
    //! \param memory       underlying memory
    explicit block_view(irk::memory_view memory)
        : memory_view_(std::move(memory))
    {
        static_assert(not supports_skips,
            "must construct with a value when parameter type defined");
    }

    //! Returns the underlying memory view.
    const irk::memory_view& data() const { return memory_view_; }

    const T& back() const
    {
        static_assert(supports_skips, "list does not support skips: no value");
        return last_value_[0];
    }

private:
    std::array<T, supports_skips ? 1 : 0> last_value_;
    irk::memory_view memory_view_;
};

}  // namespace irk::index
