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

//! \file unit_test_vector_inverted_lists.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#include <algorithm>
#include <random>
#include <sstream>
#include <vector>

#include <gmock/gmock.h>
#include <gsl/span>
#include <gtest/gtest.h>

#include <irkit/index/vector_inverted_list.hpp>

namespace {

TEST(vector_document_list, forward_iterator)
{
    std::vector<long> documents = {0, 1, 4, 6, 9, 11, 30};
    irk::vector_document_list vdl(documents);
    vdl.block_size(3);

    std::vector<long> from_list(vdl.begin(), vdl.end());
    ASSERT_THAT(from_list, ::testing::ElementsAreArray(documents));
}

TEST(vector_document_list, moveto)
{
    std::vector<long> documents = {0, 1, 4, 6, 9, 11, 30};
    irk::vector_document_list vdl(documents);
    vdl.block_size(3);

    ASSERT_EQ(*vdl.begin().moveto(0), 0);
    ASSERT_EQ(*vdl.begin().moveto(1), 1);
    ASSERT_EQ(*vdl.begin().moveto(2), 4);
    ASSERT_EQ(*vdl.begin().moveto(3), 4);
    ASSERT_EQ(*vdl.begin().moveto(4), 4);
    ASSERT_EQ(*vdl.begin().moveto(5), 6);
    ASSERT_EQ(*vdl.begin().moveto(6), 6);
    ASSERT_EQ(*vdl.begin().moveto(7), 9);
    ASSERT_EQ(*vdl.begin().moveto(8), 9);
    ASSERT_EQ(*vdl.begin().moveto(9), 9);
    ASSERT_EQ(*vdl.begin().moveto(15), 30);
    ASSERT_EQ(*vdl.begin().moveto(30), 30);
    ASSERT_EQ(vdl.begin().moveto(31), vdl.end());

    auto iter = vdl.begin();
    iter.moveto(4);
    ASSERT_EQ(*iter, 4);
    iter.moveto(8);
    ASSERT_EQ(*iter, 9);
    iter.moveto(40);
    ASSERT_EQ(iter, vdl.end());
}

TEST(vector_document_list, nextgeq)
{
    std::vector<long> documents = {0, 1, 4, 6, 9, 11, 30};
    irk::vector_document_list vdl(documents);
    vdl.block_size(3);

    ASSERT_EQ(*vdl.begin().nextgeq(0), 0);
    ASSERT_EQ(*vdl.begin().nextgeq(1), 1);
    ASSERT_EQ(*vdl.begin().nextgeq(2), 4);
    ASSERT_EQ(*vdl.begin().nextgeq(3), 4);
    ASSERT_EQ(*vdl.begin().nextgeq(4), 4);
    ASSERT_EQ(*vdl.begin().nextgeq(5), 6);
    ASSERT_EQ(*vdl.begin().nextgeq(6), 6);
    ASSERT_EQ(*vdl.begin().nextgeq(7), 9);
    ASSERT_EQ(*vdl.begin().nextgeq(8), 9);
    ASSERT_EQ(*vdl.begin().nextgeq(9), 9);
    ASSERT_EQ(*vdl.begin().nextgeq(15), 30);
    ASSERT_EQ(*vdl.begin().nextgeq(30), 30);
    ASSERT_EQ(vdl.begin().nextgeq(31), vdl.end());
}

TEST(vector_document_list, lookup)
{
    std::vector<long> documents = {0, 1, 4, 6, 9, 11, 30};
    irk::vector_document_list vdl(documents);
    vdl.block_size(3);

    ASSERT_EQ(*vdl.lookup(0), 0);
    ASSERT_EQ(*vdl.lookup(1), 1);
    ASSERT_EQ(*vdl.lookup(2), 4);
    ASSERT_EQ(*vdl.lookup(3), 4);
    ASSERT_EQ(*vdl.lookup(4), 4);
    ASSERT_EQ(*vdl.lookup(5), 6);
    ASSERT_EQ(*vdl.lookup(6), 6);
    ASSERT_EQ(*vdl.lookup(7), 9);
    ASSERT_EQ(*vdl.lookup(8), 9);
    ASSERT_EQ(*vdl.lookup(9), 9);
    ASSERT_EQ(*vdl.lookup(15), 30);
    ASSERT_EQ(*vdl.lookup(30), 30);
    ASSERT_EQ(vdl.lookup(31), vdl.end());
}

TEST(vector_payload_list, forward_iterator)
{
    std::vector<double> payloads = {0, 1, 4, 6, 9, 11, 30};
    irk::vector_payload_list vpl(payloads);
    vpl.block_size(3);

    std::vector<double> from_list(vpl.begin(), vpl.end());
    ASSERT_THAT(from_list, ::testing::ElementsAreArray(payloads));
}

TEST(vector_payload_list, alignment)
{
    std::vector<long> documents = {0, 1, 4, 6, 9, 11, 30};
    std::vector<double> payloads = {0, 1, 4, 6, 9, 11, 30};
    irk::vector_document_list vdl(documents);
    irk::vector_payload_list vpl(payloads);
    vdl.block_size(3);
    vpl.block_size(3);

    ASSERT_EQ(*vpl.at(vdl.begin()), 0.0);
    ASSERT_EQ(*vpl.at(vdl.lookup(1)), 1);
    ASSERT_EQ(*vpl.at(vdl.lookup(2)), 4);
    ASSERT_EQ(*vpl.at(vdl.lookup(3)), 4);
    ASSERT_EQ(*vpl.at(vdl.lookup(4)), 4);
    ASSERT_EQ(*vpl.at(vdl.lookup(5)), 6);
    ASSERT_EQ(*vpl.at(vdl.lookup(6)), 6);
    ASSERT_EQ(*vpl.at(vdl.lookup(7)), 9);
    ASSERT_EQ(*vpl.at(vdl.lookup(8)), 9);
    ASSERT_EQ(*vpl.at(vdl.lookup(9)), 9);
    ASSERT_EQ(*vpl.at(vdl.lookup(15)), 30);
    ASSERT_EQ(*vpl.at(vdl.lookup(30)), 30);
    ASSERT_EQ( vpl.at(vdl.lookup(31)), vpl.end());
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
