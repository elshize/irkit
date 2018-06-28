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
#include <irkit/coding/varbyte.hpp>
#include <irkit/index/block_inverted_list.hpp>

namespace {

char vb(int n)
{
    return n | (char)0b10000000;
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

    using view_type = irk::index::block_document_list_view;
    view_type view = view_type(irk::varbyte_codec<irk::index::document_t>{},
        irk::make_memory_view(
            gsl::span<const char>(&memory[3], memory.size() - 6)),
        5 /* frequency */);

    std::vector<int> documents = {9, 11, 12, 22, 27};
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

    using view_type = irk::index::block_payload_list_view<std::int32_t>;
    view_type view = view_type(irk::varbyte_codec<std::int32_t>{},
        irk::make_memory_view(
            gsl::span<const char>(&memory[3], memory.size() - 6)),
        5 /* frequency */);

    std::vector<int> payloads = {9, 2, 1, 10, 5};
};

class C_block_payload_list_view_double {
public:
    std::vector<double> scores = {0.5, 11.9, 4.3, 1.6, 0.01};
    std::vector<char> memory = {
        /* The following will be read and decoded on construction of a view */
        vb(46), /* size of the memory in bytes */
        vb(2), /* block size */
        vb(3), /* number of blocks */
        vb(0), vb(16), vb(16) /* skips (relative to previous block) */
    };

    using view_type = irk::index::block_payload_list_view<double>;
    view_type view;

    C_block_payload_list_view_double()
    {
        memory.insert(memory.end(),
            reinterpret_cast<char*>(&scores[0]),
            reinterpret_cast<char*>(&scores[0]) + 5 * sizeof(double));
        view = view_type(irk::copy_codec<double>{},
            irk::make_memory_view(
                gsl::span<const char>(&memory[0], memory.size())),
            5 /* frequency */);
    }
};

class block_document_list_view : public ::testing::Test {
protected:
    C_block_document_list_view data;
};
class block_payload_list_view : public ::testing::Test {
protected:
    C_block_payload_list_view data;
};
class zipped_payload_list_view : public ::testing::Test {
protected:
    C_block_payload_list_view payload_data_int;
    C_block_payload_list_view_double payload_data_double;
    irk::index::zipped_payload_list_view<int, double, int> zipped =
        irk::index::zipped_payload_list_view(
            payload_data_int.view,
            payload_data_double.view,
            payload_data_int.view);
};
class block_list_builder : public ::testing::Test {
protected:
    C_block_document_list_view doc_data;
    C_block_payload_list_view pay_data;
    C_block_payload_list_view_double double_data;
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
    std::vector<int> documents(data.view.begin(), data.view.end());
    EXPECT_THAT(documents, ::testing::ElementsAreArray(data.documents));
}

TEST_F(block_document_list_view, next_ge_block)
{
    ASSERT_EQ(data.view.begin().nextgeq_block(0, 0), 0);
    ASSERT_EQ(data.view.begin().nextgeq_block(0, 9), 0);
    ASSERT_EQ(data.view.begin().nextgeq_block(0, 11), 0);
    ASSERT_EQ(data.view.begin().nextgeq_block(0, 12), 1);
    ASSERT_EQ(data.view.begin().nextgeq_block(0, 13), 1);
    ASSERT_EQ(data.view.begin().nextgeq_block(0, 22), 1);
    ASSERT_EQ(data.view.begin().nextgeq_block(0, 23), 2);
    ASSERT_EQ(data.view.begin().nextgeq_block(0, 27), 2);
    ASSERT_EQ(data.view.begin().nextgeq_block(0, 101), 3);  // after last block
}

TEST_F(block_document_list_view, next_ge)
{
    ASSERT_EQ(*data.view.begin().nextgeq(0), 9);
    ASSERT_EQ(*data.view.begin().nextgeq(10), 11);
    ASSERT_EQ(*data.view.begin().nextgeq(11), 11);
    ASSERT_EQ(*data.view.begin().nextgeq(12), 12);
    ASSERT_EQ(*data.view.begin().nextgeq(14), 22);
    ASSERT_EQ(data.view.begin().nextgeq(101), data.view.end());
    ASSERT_EQ(*data.view.begin().nextgeq(0).nextgeq(0).nextgeq(10).nextgeq(15), 22);
}

TEST_F(block_document_list_view, copy)
{
    auto test_wrapper = [=](irk::index::block_document_list_view view) {
        std::vector<int> documents(view.begin(), view.end());
        EXPECT_THAT(documents, ::testing::ElementsAreArray(data.documents));
    };
    test_wrapper(data.view);
}

TEST_F(block_document_list_view, move)
{
    irk::index::block_document_list_view copy(std::move(data.view));
    std::vector<int> documents(copy.begin(), copy.end());
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

TEST_F(zipped_payload_list_view, read_iterator_loop)
{
    std::vector<int> payloads_1;
    std::vector<double> payloads_2;
    std::vector<int> payloads_3;
    for (auto pos = zipped.begin(); pos != zipped.end(); pos++) {
        payloads_1.push_back(pos.payload<0>());
        payloads_2.push_back(pos.payload<1>());
        payloads_3.push_back(pos.payload<2>());
    }
    EXPECT_THAT(
        payloads_1, ::testing::ElementsAreArray(payload_data_int.payloads));
    EXPECT_THAT(
        payloads_2, ::testing::ElementsAreArray(payload_data_double.scores));
    EXPECT_THAT(
        payloads_3, ::testing::ElementsAreArray(payload_data_int.payloads));
}

TEST_F(zipped_payload_list_view, read_iterator_copy)
{
    std::vector<std::tuple<int, double, int>> payloads(zipped.begin(), zipped.end());
    std::vector<std::tuple<int, double, int>> expected = {
        {9, 0.5, 9}, {2, 11.9, 2}, {1, 4.3, 1}, {10, 1.6, 10}, {5, 0.01, 5}};
    EXPECT_THAT(payloads, ::testing::ElementsAreArray(expected));
}

TEST_F(block_list_builder, write_docs)
{
    irk::index::block_list_builder<int, true> builder(
        2 /* block_size */, irk::varbyte_codec<int>{});
    std::ostringstream buffer;
    for (int doc : doc_data.documents) { builder.add(doc); }
    builder.write(buffer);

    std::vector<char> expected(
        doc_data.memory.begin() + 3, doc_data.memory.end() - 3);

    EXPECT_THAT(buffer.str(), ::testing::ElementsAreArray(expected));
}

TEST_F(block_list_builder, write_payloads)
{
    irk::index::block_list_builder<int, false> builder(
        2 /* block_size */, irk::varbyte_codec<int>{});
    std::ostringstream buffer;
    for (int pay : pay_data.payloads) { builder.add(pay); }
    builder.write(buffer);

    std::vector<char> expected(
        pay_data.memory.begin() + 3, pay_data.memory.end() - 3);

    EXPECT_THAT(buffer.str(), ::testing::ElementsAreArray(expected));
}

TEST_F(block_list_builder, write_double_payloads)
{
    irk::index::block_list_builder<double, false> builder(
        2 /* block_size */, irk::copy_codec<double>{});
    std::ostringstream buffer;
    for (double pay : double_data.scores) { builder.add(pay); }
    builder.write(buffer);

    std::vector<char>
        expected(double_data.memory.begin(), double_data.memory.begin() + 46);
    EXPECT_THAT(buffer.str(), ::testing::ElementsAreArray(expected));
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
