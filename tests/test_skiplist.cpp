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

//! \file test_skiplist.hpp
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
#include <irkit/index/skiplist.hpp>

namespace {

using namespace irk::index;

class int_skip_list: public ::testing::Test {
protected:
    std::vector<int> intlist = {5, // size
        0, 2, 1, 10, 5};
    skip_list_view<int> view = skip_list_view<int>(irk::make_memory_view(
        reinterpret_cast<char*>(&intlist[0]), sizeof(int) * intlist.size()));
};

TEST_F(int_skip_list, iterator)
{
    std::vector<int> from_iterator(view.begin(), view.end());
    std::vector<int> expected(intlist.begin() + 1, intlist.end());
    ASSERT_EQ(view.size(), expected.size() * sizeof(int));
    EXPECT_THAT(from_iterator, ::testing::ElementsAreArray(expected));
}

class id_skip_list: public ::testing::Test {
protected:
    std::vector<int> idlist = {5, // size
        0, 0, 10, 2, 15, 1, 20, 10, 100, 5};
    irk::memory_view mem = irk::make_memory_view(
        reinterpret_cast<char*>(&idlist[0]), sizeof(int) * idlist.size());
    skip_list_view<idskip<int, int>> view = skip_list_view<idskip<int, int>>(
        mem);
};

TEST_F(id_skip_list, iterator)
{
    std::vector<idskip<int, int>> from_iterator(view.begin(), view.end());
    std::vector<idskip<int, int>> expected = {
        {0, 0}, {10, 2}, {15, 1}, {20, 10}, {100, 5}};
    ASSERT_EQ(view.size(), expected.size() * sizeof(idskip<int, int>));
    EXPECT_THAT(from_iterator, ::testing::ElementsAreArray(expected));
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}


