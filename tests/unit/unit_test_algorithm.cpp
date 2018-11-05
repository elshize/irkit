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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <irkit/algorithm/accumulate.hpp>

namespace {

TEST(accumulate_while, binary_op)
{
    std::vector<std::pair<int, double>> vec{
        {0, 1.0}, {0, 2.0}, {1, 4.0}, {10, 1.0}, {10, 6.0}, {10, 1.0}};
    auto [acc, pos] = irk::accumulate_while(
        vec.begin(),
        vec.end(),
        0.0,
        [&](const auto& tup) { return tup.first == 0; },
        [&](const auto& acc, const auto& tup) { return acc + tup.second; });
    ASSERT_EQ(acc, 3.0);
    std::tie(acc, pos) = irk::accumulate_while(
        pos,
        vec.end(),
        0.0,
        [&](const auto& tup) { return tup.first == 1; },
        [&](const auto& acc, const auto& tup) { return acc + tup.second; });
    ASSERT_EQ(acc, 4.0);
    std::tie(acc, pos) = irk::accumulate_while(
        pos,
        vec.end(),
        0.0,
        [&](const auto& tup) { return tup.first == 10; },
        [&](const auto& acc, const auto& tup) { return acc + tup.second; });
    ASSERT_EQ(acc, 8.0);
}

TEST(accumulate_while, count)
{
    std::vector<int> vec{0, 0, 0, 1, 4, 4};
    auto [count, pos] = irk::accumulate_while(
        vec.begin(),
        vec.end(),
        0,
        [&](const auto& v) { return v == 0; },
        [&](const auto& acc, const auto& v) { return acc + 1; });
    ASSERT_EQ(count, 3);
    std::tie(count, pos) = irk::accumulate_while(
        pos,
        vec.end(),
        0,
        [&](const auto& v) { return v == 0; },
        [&](const auto& acc, const auto& v) { return acc + 1; });
    ASSERT_EQ(count, 0);
    std::tie(count, pos) = irk::accumulate_while(
        pos,
        vec.end(),
        0,
        [&](const auto& v) { return v == 1; },
        [&](const auto& acc, const auto& v) { return acc + 1; });
    ASSERT_EQ(count, 1);
    std::tie(count, pos) = irk::accumulate_while(
        pos,
        vec.end(),
        0,
        [&](const auto& v) { return v == 4; },
        [&](const auto& acc, const auto& v) { return acc + 1; });
    ASSERT_EQ(count, 2);
}

}  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
