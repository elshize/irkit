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
//! \author Michal Siedlaczek
//! \copyright MIT License

#include <algorithm>
#include <sstream>
#include <vector>

#include <gmock/gmock.h>
#include <gsl/span>
#include <gtest/gtest.h>

#define private public
#define protected public
#include <irkit/index/vector_inverted_list.hpp>
#include <irkit/index/posting_list.hpp>

namespace {

TEST(posting_list_view, forward_iterator)
{
    std::vector<long> documents = {0, 1, 4, 6, 9, 11, 30};
    std::vector<double> payloads = {0, 1, 4, 6, 9, 11, 30};
    irk::vector_document_list vdl(documents);
    irk::vector_payload_list vpl(payloads);
    vdl.block_size(3);
    vpl.block_size(3);

    irk::posting_list_view postings(vdl, vpl);
    int idx = 0;
    for (auto posting : postings) {
        ASSERT_EQ(posting.document(), documents[idx]);
        ASSERT_EQ(posting.payload(), payloads[idx]);
        idx++;
    }
}

template<class T>
std::pair<long, double> pair_of(const T& posting)
{
    long doc = posting->document();
    double pay = posting->payload();
    return std::make_pair(doc, pay);
}

TEST(posting_list_view, lookup)
{
    std::vector<long> documents = {0, 1, 4, 6, 9, 11, 30};
    std::vector<double> payloads = {0, 1, 4, 6, 9, 11, 30.1};
    irk::vector_document_list vdl(documents);
    irk::vector_payload_list vpl(payloads);
    vdl.block_size(3);
    vpl.block_size(3);

    irk::posting_list_view postings(vdl, vpl);

    std::vector<std::pair<long, double>> p;
    for (auto it = postings.begin(); it != postings.end(); ++it) {
        p.emplace_back(it->document(), it->payload());
    }
    std::vector<std::pair<long, double>> e = {{0, 0}, {1, 1}, {4, 4}, {6, 6}, {9, 9}, {11, 11}, {30, 30.1}};
    ASSERT_THAT(p, ::testing::ElementsAreArray(e));

    ASSERT_THAT(pair_of(postings.lookup(1)), ::testing::Pair(1, 1));
    ASSERT_THAT(pair_of(postings.lookup(2)), ::testing::Pair(4, 4));
    ASSERT_THAT(pair_of(postings.lookup(3)), ::testing::Pair(4, 4));
    ASSERT_THAT(pair_of(postings.lookup(4)), ::testing::Pair(4, 4));
    ASSERT_THAT(pair_of(postings.lookup(5)), ::testing::Pair(6, 6));
    ASSERT_THAT(pair_of(postings.lookup(6)), ::testing::Pair(6, 6));
    ASSERT_THAT(pair_of(postings.lookup(7)), ::testing::Pair(9, 9));
    ASSERT_THAT(pair_of(postings.lookup(8)), ::testing::Pair(9, 9));
    ASSERT_THAT(pair_of(postings.lookup(9)), ::testing::Pair(9, 9));
    ASSERT_THAT(pair_of(postings.lookup(15)), ::testing::Pair(30, 30.1));
    ASSERT_THAT(pair_of(postings.lookup(30)), ::testing::Pair(30, 30.1));
    ASSERT_EQ(postings.lookup(31), postings.end());
}

//TEST(posting_list_view, pair_conversion)
//{
//    std::vector<long> documents = {0, 1, 4, 6, 9, 11, 30};
//    std::vector<double> payloads = {0, 1, 4, 6, 9, 11, 30.1};
//    irk::vector_document_list vdl(documents);
//    irk::vector_payload_list vpl(payloads);
//    vdl.block_size(3);
//    vpl.block_size(3);
//
//    irk::posting_list_view postings(vdl, vpl);
//    std::vector<std::pair<long, double>> pairs(
//        postings.begin(), postings.end());
//    std::vector<std::pair<long, double>> expected = {
//        {0, 0}, {1, 1}, {4, 4}, {6, 6}, {9, 9}, {11, 11}, {30, 30.1}};
//    ASSERT_THAT(pairs, ::testing::ElementsAreArray(expected));
//}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
