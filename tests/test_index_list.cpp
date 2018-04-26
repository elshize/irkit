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

//! \file test_documentlist.hpp
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
#include <irkit/index/list.hpp>

namespace {

char vb(int n)
{
    return n | (char)0b10000000;
};

class block_document_list_view : public ::testing::Test {
protected:
    int block_size = 2;
    std::vector<char> memory = {
        5, 127, -128, /* some random bytes before posting list */

        /* The following will be read and decoded on construction of a view */
        vb(14), /* size of the memory in bytes */
        vb(2), /* block size */
        vb(3), /* number of blocks */
        vb(0), vb(2), vb(2), /* skips (relative to previous block) */
        vb(11), vb(11), vb(5), /* last values in blocks (delta encoded) */

        /* The following (ID gaps) will be read lazily */
        vb(9), vb(2), vb(1), vb(10), vb(5),

        5, 127, -128 /* some random bytes after posting list */
    };

    using view_type = irk::index::block_document_list_view<
        irk::coding::varbyte_codec<std::int32_t>>;
    view_type view = view_type(
        irk::make_memory_view(gsl::span<const char>(&memory[0], memory.size())),
        5, /* frequency */
        3 /* offset */);
};

class block_payload_list_view : public ::testing::Test {
protected:
    int block_size = 2;
    std::vector<char> memory = {
        5, 127, -128, /* some random bytes before posting list */

        /* The following will be read and decoded on construction of a view */
        vb(11), /* size of the memory in bytes */
        vb(2), /* block size */
        vb(3), /* number of blocks */
        vb(0), vb(2), vb(2), /* skips (relative to previous block) */

        /* The following payload will be read lazily */
        vb(9), vb(2), vb(1), vb(10), vb(5),

        5, 127, -128 /* some random bytes after posting list */
    };

    using view_type = irk::index::block_payload_list_view<
        irk::coding::varbyte_codec<std::int32_t>>;
    view_type view = view_type(
        irk::make_memory_view(gsl::span<const char>(&memory[0], memory.size())),
        5, /* frequency */
        3 /* offset */);
};

using namespace irk::index;

TEST_F(block_document_list_view, read_blocks_mem)
{
    ASSERT_EQ(view.blocks_[0].data().size(), 2);
    std::vector<char> b0(
        view.blocks_[0].data().begin(), view.blocks_[0].data().end());
    std::vector<char> b0_exp = {vb(9), vb(2)};
    EXPECT_THAT(b0, ::testing::ElementsAreArray(b0_exp));

    ASSERT_EQ(view.blocks_[1].data().size(), 2);
    std::vector<char> b1(
        view.blocks_[1].data().begin(), view.blocks_[1].data().end());
    std::vector<char> b1_exp = {vb(1), vb(10)};
    EXPECT_THAT(b1, ::testing::ElementsAreArray(b1_exp));

    ASSERT_EQ(view.blocks_[2].data().size(), 1);
    std::vector<char> b2(
        view.blocks_[2].data().begin(), view.blocks_[2].data().end());
    std::vector<char> b2_exp = {vb(5)};
    EXPECT_THAT(b2, ::testing::ElementsAreArray(b2_exp));
}

TEST_F(block_document_list_view, read_iterator)
{
    std::vector<int> documents(view.begin(), view.end());
    std::vector<int> expected = {9, 11, 12, 22, 27};
    EXPECT_THAT(documents, ::testing::ElementsAreArray(expected));
}

TEST_F(block_document_list_view, next_ge_block)
{
    ASSERT_EQ(view.begin().next_ge_block(0, 0), 0);
    ASSERT_EQ(view.begin().next_ge_block(0, 9), 0);
    ASSERT_EQ(view.begin().next_ge_block(0, 11), 0);
    ASSERT_EQ(view.begin().next_ge_block(0, 12), 1);
    ASSERT_EQ(view.begin().next_ge_block(0, 13), 1);
    ASSERT_EQ(view.begin().next_ge_block(0, 22), 1);
    ASSERT_EQ(view.begin().next_ge_block(0, 23), 2);
    ASSERT_EQ(view.begin().next_ge_block(0, 27), 2);
    ASSERT_EQ(view.begin().next_ge_block(0, 101), 3);  // after last block
}

TEST_F(block_document_list_view, next_ge)
{
    ASSERT_EQ(*view.begin().next_ge(0), 9);
    ASSERT_EQ(*view.begin().next_ge(10), 11);
    ASSERT_EQ(*view.begin().next_ge(11), 11);
    ASSERT_EQ(*view.begin().next_ge(12), 12);
    ASSERT_EQ(*view.begin().next_ge(14), 22);
    ASSERT_EQ(view.begin().next_ge(101), view.end());
    ASSERT_EQ(*view.begin().next_ge(0).next_ge(0).next_ge(10).next_ge(15), 22);
}

TEST_F(block_payload_list_view, read_blocks_mem)
{
    ASSERT_EQ(view.blocks_[0].data().size(), 2);
    std::vector<char> b0(
        view.blocks_[0].data().begin(), view.blocks_[0].data().end());
    std::vector<char> b0_exp = {vb(9), vb(2)};
    EXPECT_THAT(b0, ::testing::ElementsAreArray(b0_exp));

    ASSERT_EQ(view.blocks_[1].data().size(), 2);
    std::vector<char> b1(
        view.blocks_[1].data().begin(), view.blocks_[1].data().end());
    std::vector<char> b1_exp = {vb(1), vb(10)};
    EXPECT_THAT(b1, ::testing::ElementsAreArray(b1_exp));

    ASSERT_EQ(view.blocks_[2].data().size(), 1);
    std::vector<char> b2(
        view.blocks_[2].data().begin(), view.blocks_[2].data().end());
    std::vector<char> b2_exp = {vb(5)};
    EXPECT_THAT(b2, ::testing::ElementsAreArray(b2_exp));
}

TEST_F(block_payload_list_view, read_iterator)
{
    std::vector<int> documents(view.begin(), view.end());
    std::vector<int> expected = {9, 2, 1, 10, 5};
    EXPECT_THAT(documents, ::testing::ElementsAreArray(expected));
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
