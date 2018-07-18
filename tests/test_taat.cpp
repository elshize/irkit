#include <vector>
#include <unordered_map>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gsl/span"

#include "irkit/taat.hpp"

namespace {

class taat : public ::testing::Test {
protected:
    std::vector<int> docs = {2, 1, 0};
    std::vector<int> scores = {1, 2, 3};
    std::vector<int> acc = {0, 0, 0, 0};
};

TEST_F(taat, vectors)
{
    irk::accumulate(docs, scores, acc);
    EXPECT_THAT(acc, ::testing::ElementsAreArray({3, 2, 1, 0}));
    auto top = irk::aggregate_top_k<int, int>(acc, 2);
    EXPECT_THAT(top[0], ::testing::Pair(0, 3));
    EXPECT_THAT(top[1], ::testing::Pair(1, 2));
}

TEST_F(taat, block_accumulator_vector)
{
    irk::block_accumulator_vector<int> bacc(4, 2);
    irk::accumulate(docs, scores, bacc);
    EXPECT_THAT(bacc.accumulators, ::testing::ElementsAreArray({3, 2, 1, 0}));
    EXPECT_THAT(bacc.max_values, ::testing::ElementsAreArray({3, 1}));
    auto top = irk::aggregate_top_k<int, int>(bacc, 2);
    EXPECT_THAT(top[0], ::testing::Pair(0, 3));
    EXPECT_THAT(top[1], ::testing::Pair(1, 2));
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
