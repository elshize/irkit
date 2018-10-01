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

#include <algorithm>
#include <random>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <irkit/compacttable.hpp>

namespace {

TEST(offset_table, big_values)
{
    std::vector<std::size_t> values = {0, 213, 12'148'409'321};
    auto table = irk::build_offset_table(values);
    ASSERT_EQ(table[0], values[0]);
    ASSERT_EQ(table[1], values[1]);
    ASSERT_EQ(table[2], values[2]);
}

TEST(offset_table, iterator)
{
    // given
    std::default_random_engine generator(127);
    std::uniform_int_distribution<std::size_t> distribution(
        0, std::numeric_limits<std::int32_t>::max());
    std::vector<std::size_t> values;
    int count = 10000;
    values.reserve(count);
    std::generate_n(std::back_inserter(values), count, [&]() {
        return distribution(generator);
    });
    std::sort(std::begin(values), std::end(values));

    // when
    auto table = irk::build_offset_table(values);
    std::vector<std::size_t> vec;
    for (auto iter = table.begin(); iter != table.end(); ++iter) {
        vec.push_back(*iter);
    }

    // then
    ASSERT_THAT(vec, ::testing::ElementsAreArray(values));
}

TEST(offset_table, to_vector)
{
    // given
    std::default_random_engine generator(127);
    std::uniform_int_distribution<std::size_t> distribution(
        0, std::numeric_limits<std::int32_t>::max());
    std::vector<std::size_t> values;
    int count = 10000;
    values.reserve(count);
    std::generate_n(std::back_inserter(values), count, [&]() {
        return distribution(generator);
    });
    std::sort(std::begin(values), std::end(values));

    // when
    auto table = irk::build_offset_table(values);
    auto vec = table.to_vector();

    // then
    ASSERT_THAT(vec, ::testing::ElementsAreArray(values));
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
