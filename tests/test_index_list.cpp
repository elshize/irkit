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
#include <sstream>
#include <vector>

#include <gmock/gmock.h>
#include <gsl/span>
#include <gtest/gtest.h>

#define private public
#define protected public
#include <irkit/coding/copy.hpp>
#include <irkit/coding/vbyte.hpp>
#include <irkit/index/block_inverted_list.hpp>

namespace {

using irk::index::document_t;
using irk::index::operator""_id;

char vb(int n)
{
    return n | (char)0b10000000;
};

char rvb(int n)
{
    return n & (char)0b01111111;
};

class C_block_document_list_view {
public:
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
        irk::vbyte_codec<irk::index::document_t>>;
    view_type view = view_type(irk::make_memory_view(gsl::span<const char>(
                                   &memory[3], memory.size() - 6)),
        5 /* frequency */);

    std::vector<document_t> documents = {9_id, 11_id, 12_id, 22_id, 27_id};
};

class C_block_payload_list_view {
public:
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

    using view_type = irk::index::block_payload_list_view<std::int32_t,
        irk::vbyte_codec<std::int32_t>>;
    view_type view = view_type(irk::make_memory_view(gsl::span<const char>(
                                   &memory[3], memory.size() - 6)),
        5 /* frequency */);

    std::vector<std::int32_t> payloads = {9, 2, 1, 10, 5};
};

// TODO: create new-style copy codec
//class C_block_payload_list_view_double {
//public:
//    std::vector<double> scores = {0.5, 11.9, 4.3, 1.6, 0.01};
//    std::vector<char> memory = {
//        /* The following will be read and decoded on construction of a view */
//        vb(46), /* size of the memory in bytes */
//        vb(2), /* block size */
//        vb(3), /* number of blocks */
//        vb(0), vb(16), vb(16) /* skips (relative to previous block) */
//    };
//
//    using view_type = irk::index::block_payload_list_view<double>;
//    view_type view;
//
//    C_block_payload_list_view_double()
//    {
//        memory.insert(memory.end(),
//            reinterpret_cast<char*>(&scores[0]),
//            reinterpret_cast<char*>(&scores[0]) + 5 * sizeof(double));
//        view = view_type(irk::copy_codec<double>{},
//            irk::make_memory_view(
//                gsl::span<const char>(&memory[0], memory.size())),
//            5 /* frequency */);
//    }
//};

class block_document_list_view : public ::testing::Test {
protected:
    C_block_document_list_view data;
};
class block_payload_list_view : public ::testing::Test {
protected:
    C_block_payload_list_view data;
};
class block_list_builder : public ::testing::Test {
protected:
    C_block_document_list_view doc_data;
    C_block_payload_list_view pay_data;
};

using namespace irk::index;

TEST_F(block_document_list_view, read_blocks_mem)
{
    ASSERT_EQ(data.view.blocks_[0].data().size(), 2);
    std::vector<char> b0(
        data.view.blocks_[0].data().begin(), data.view.blocks_[0].data().end());
    std::vector<char> b0_exp = {vb(9), vb(2)};
    EXPECT_THAT(b0, ::testing::ElementsAreArray(b0_exp));

    ASSERT_EQ(data.view.blocks_[1].data().size(), 2);
    std::vector<char> b1(
        data.view.blocks_[1].data().begin(), data.view.blocks_[1].data().end());
    std::vector<char> b1_exp = {vb(1), vb(10)};
    EXPECT_THAT(b1, ::testing::ElementsAreArray(b1_exp));

    ASSERT_EQ(data.view.blocks_[2].data().size(), 1);
    std::vector<char> b2(
        data.view.blocks_[2].data().begin(), data.view.blocks_[2].data().end());
    std::vector<char> b2_exp = {vb(5)};
    EXPECT_THAT(b2, ::testing::ElementsAreArray(b2_exp));
}

TEST_F(block_document_list_view, read_iterator)
{
    std::vector<document_t> documents(data.view.begin(), data.view.end());
    EXPECT_THAT(documents, ::testing::ElementsAreArray(data.documents));
}

TEST_F(block_document_list_view, next_ge_block)
{
    ASSERT_EQ(data.view.begin().nextgeq_block(0, 0_id), 0);
    ASSERT_EQ(data.view.begin().nextgeq_block(0, 9_id), 0);
    ASSERT_EQ(data.view.begin().nextgeq_block(0, 11_id), 0);
    ASSERT_EQ(data.view.begin().nextgeq_block(0, 12_id), 1);
    ASSERT_EQ(data.view.begin().nextgeq_block(0, 13_id), 1);
    ASSERT_EQ(data.view.begin().nextgeq_block(0, 22_id), 1);
    ASSERT_EQ(data.view.begin().nextgeq_block(0, 23_id), 2);
    ASSERT_EQ(data.view.begin().nextgeq_block(0, 27_id), 2);
    ASSERT_EQ(data.view.begin().nextgeq_block(0, 101_id), 3);  // after last block
}

TEST_F(block_document_list_view, next_ge)
{
    ASSERT_EQ(*data.view.begin().nextgeq(0_id), 9_id);
    ASSERT_EQ(*data.view.begin().nextgeq(10_id), 11_id);
    ASSERT_EQ(*data.view.begin().nextgeq(11_id), 11_id);
    ASSERT_EQ(*data.view.begin().nextgeq(12_id), 12_id);
    ASSERT_EQ(*data.view.begin().nextgeq(14_id), 22_id);
    ASSERT_EQ(data.view.begin().nextgeq(101_id), data.view.end());
    ASSERT_EQ(*data.view.begin().nextgeq(0_id)
        .nextgeq(0_id).nextgeq(10_id).nextgeq(15_id), 22_id);
}

TEST_F(block_document_list_view, copy)
{
    auto test_wrapper = [=](irk::index::block_document_list_view<
                            irk::vbyte_codec<irk::index::document_t>> view) {
        std::vector<document_t> documents(view.begin(), view.end());
        EXPECT_THAT(documents, ::testing::ElementsAreArray(data.documents));
    };
    test_wrapper(data.view);
}

TEST_F(block_document_list_view, move)
{
    irk::index::block_document_list_view copy(std::move(data.view));
    std::vector<document_t> documents(copy.begin(), copy.end());
    EXPECT_THAT(documents, ::testing::ElementsAreArray(data.documents));
}

TEST_F(block_payload_list_view, read_blocks_mem)
{
    ASSERT_EQ(data.view.blocks_[0].data().size(), 2);
    std::vector<char> b0(
        data.view.blocks_[0].data().begin(), data.view.blocks_[0].data().end());
    std::vector<char> b0_exp = {vb(9), vb(2)};
    EXPECT_THAT(b0, ::testing::ElementsAreArray(b0_exp));

    ASSERT_EQ(data.view.blocks_[1].data().size(), 2);
    std::vector<char> b1(
        data.view.blocks_[1].data().begin(), data.view.blocks_[1].data().end());
    std::vector<char> b1_exp = {vb(1), vb(10)};
    EXPECT_THAT(b1, ::testing::ElementsAreArray(b1_exp));

    ASSERT_EQ(data.view.blocks_[2].data().size(), 1);
    std::vector<char> b2(
        data.view.blocks_[2].data().begin(), data.view.blocks_[2].data().end());
    std::vector<char> b2_exp = {vb(5)};
    EXPECT_THAT(b2, ::testing::ElementsAreArray(b2_exp));
}

TEST_F(block_payload_list_view, read_iterator)
{
    std::vector<int> payloads(data.view.begin(), data.view.end());
    EXPECT_THAT(payloads, ::testing::ElementsAreArray(data.payloads));
}

TEST_F(block_list_builder, write_docs)
{
    irk::index::
        block_list_builder<document_t, irk::vbyte_codec<document_t>, true>
            builder(2 /* block_size */);
    std::ostringstream buffer;
    for (auto doc : doc_data.documents) { builder.add(doc); }
    builder.write(buffer);

    std::vector<char> expected(
        doc_data.memory.begin() + 3, doc_data.memory.end() - 3);

    auto x = buffer.str();
    EXPECT_THAT(buffer.str(), ::testing::ElementsAreArray(expected));
}

TEST_F(block_list_builder, write_payloads)
{
    irk::index::block_list_builder<int, irk::vbyte_codec<int>, false> builder(
        2 /* block_size */);
    std::ostringstream buffer;
    for (int pay : pay_data.payloads) { builder.add(pay); }
    builder.write(buffer);

    std::vector<char> expected(
        pay_data.memory.begin() + 3, pay_data.memory.end() - 3);

    EXPECT_THAT(buffer.str(), ::testing::ElementsAreArray(expected));
}

//TEST_F(block_list_builder, write_double_payloads)
//{
//    irk::index::block_list_builder<double, false> builder(
//        2 /* block_size */, irk::copy_codec<double>{});
//    std::ostringstream buffer;
//    for (double pay : double_data.scores) { builder.add(pay); }
//    builder.write(buffer);
//
//    std::vector<char>
//        expected(double_data.memory.begin(), double_data.memory.begin() + 46);
//    EXPECT_THAT(buffer.str(), ::testing::ElementsAreArray(expected));
//}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
