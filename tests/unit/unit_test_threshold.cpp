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

#include <random>

#include <cppitertools/itertools.hpp>
#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <irkit/algorithm/transform.hpp>
#include <irkit/taat.hpp>
#include <irkit/threshold.hpp>

namespace {

TEST(threshold, compute_threshold)
{
    std::vector<std::vector<int>> documents = {{0, 3, 6}, {0, 1, 3}};
    std::vector<std::vector<int>> scores = {{3, 1, 2}, {3, 1, 2}};
    ASSERT_EQ(
        irk::compute_threshold(
            documents.begin(),
            documents.end(),
            scores.begin(),
            scores.end(),
            3),
        2);
}

TEST(threshold, same_as_taat)
{
    // given
    int length = 10000;
    int ndoc = 50000;
    int k = 10;
    std::mt19937 gen(17);
    std::uniform_int_distribution<> score_dis(0, 8);
    std::vector<std::vector<int>> documents(5);
    std::vector<std::vector<int>> scores(5);
    for (auto&& [docs, scos] : iter::zip(documents, scores)) {
        docs.resize(ndoc);
        std::iota(docs.begin(), docs.end(), 0);
        std::shuffle(docs.begin(), docs.end(), gen);
        docs.resize(length);
        std::sort(docs.begin(), docs.end());
        scos.resize(length);
        irk::transform_range(
            scos, scos.begin(), [&](int) { return score_dis(gen); });
    }

    // when
    std::vector<int> acc(ndoc, 0);
    for (auto&& [docs, scos] : iter::zip(documents, scores)) {
        for (auto&& [doc, score] : iter::zip(docs, scos)) {
            acc[doc] += score;
        }
    }
    auto x = irk::aggregate_top_k<int, int>(acc, k);
    auto taat_threshold = x.back().second;
    auto threshold = irk::compute_threshold(
        documents.begin(), documents.end(), scores.begin(), scores.end(), k);

    ASSERT_EQ(threshold, taat_threshold);
}

}  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
