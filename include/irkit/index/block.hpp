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

//! \file block.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <gsl/span>
#include <irkit/coding.hpp>
#include <irkit/memoryview.hpp>
#include <optional>
#include <vector>

namespace irk::index
{

//! \deprecated A view to an encoded block.
/*!
 * \tparam  T   the type of values encoded in the block
 */
template<class T = std::nullopt_t>
class block_view {
public:
    using value_type = T;
    using char_type = char;
    bool constexpr static supports_skips = !std::is_same_v<T, std::nullopt_t>;

    block_view(T last_value, irk::memory_view memory)
        : memory_view_(memory)
    {
        static_assert(supports_skips,
            "must define parameter type to construct with a value");
        last_value_[0] = last_value;
    }

    block_view(irk::memory_view memory) : memory_view_(memory)
    {
        static_assert(!supports_skips,
            "must construct with a value when parameter type defined");
    }

    const irk::memory_view& data() const { return memory_view_; }

    template<class = std::enable_if_t<supports_skips>>
    const T& back() const { return last_value_[0]; }

private:
    std::array<T, supports_skips> last_value_;
    irk::memory_view memory_view_;
};

};  // namespace irk::index
