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

#include <tuple>

namespace irk {

namespace detail {

    template<class InputIt, class UnaryFunction>
    struct GroupBy;

    template<class InputIt, class UnaryFunction, class AggFunction, class AggType>
    struct GroupByAggregate {
        using value_type =
            std::remove_reference_t<decltype(*std::declval<InputIt>())>;
        const InputIt first;
        const InputIt last;
        const UnaryFunction group_fn;
        const AggFunction agg_fn;
        const AggType init;

        template<class Function>
        void for_each(Function f) const
        {
            auto g = group_fn;
            AggType agg_value{};
            auto pos = first;
            while (pos != last) {
                auto current = group_fn(*pos);
                std::tie(agg_value, pos) = accumulate_while(
                    pos,
                    last,
                    init,
                    [current, g](const auto& elem) {
                        return g(elem) == current;
                    },
                    agg_fn);
                f(current, agg_value);
            }
        }
    };

    template<class InputIt, class UnaryFunction>
    struct GroupBy {
        InputIt first;
        InputIt last;
        UnaryFunction group_fn;
        GroupBy(InputIt first, InputIt last, UnaryFunction f)
            : first(std::move(first)),
              last(std::move(last)),
              group_fn(std::move(f))
        {}

        template<class T, class BinaryOperation>
        auto aggregate_groups(BinaryOperation op, T init) const
        {
            return detail::
                GroupByAggregate<InputIt, UnaryFunction, BinaryOperation, T>{
                    first, last, group_fn, op, init};
        }
    };

}  // namespace detail

template<class InputIt, class UnaryFunction>
auto group_by(InputIt first, InputIt last, UnaryFunction f)
{
    return detail::GroupBy(std::move(first), std::move(last), std::move(f));
}

}  // namespace irk
