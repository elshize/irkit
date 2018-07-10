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

//! \file test_block.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#include <algorithm>
#include <gsl/span>
#include <random>
#include <sstream>
#include <vector>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#define private public
#define protected public
#include <irkit/coding/varbyte.hpp>
#include <irkit/index/block.hpp>
#include <irkit/memoryview.hpp>

namespace {

using namespace irk::index;

TEST(block_view, int_skips)
{
    std::vector<char> items = {4, 2, 1, 4, 6};
    auto source = irk::make_memory_view(items);
    auto mem = irk::memory_view(source);
    block_view block(6, mem);
    auto block_mem = block.data();
    std::vector<char> decoded(block_mem.begin(), block_mem.end());
    EXPECT_EQ(block.back(), 6);
    EXPECT_THAT(decoded, ::testing::ElementsAreArray(items));
}

TEST(block_view, no_skips)
{
    std::vector<char> items = {4, 2, 1, 4, 6};
    auto source = irk::make_memory_view(items);
    auto mem = irk::memory_view(source);
    block_view block(mem);
    auto block_mem = block.data();
    std::vector<char> decoded(block_mem.begin(), block_mem.end());
    EXPECT_THAT(decoded, ::testing::ElementsAreArray(items));
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
