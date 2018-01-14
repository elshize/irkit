#include <vector>
#include <unordered_map>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gsl/span"
#include "irkit/taat.hpp"

namespace {

class TraverseList : public ::testing::Test {
protected:
    std::vector<int> docs = {1, 2, 3};
    std::vector<int> scores = {1, 2, 3};
    std::vector<int> acc = {0, 0, 0, 0};
};

TEST_F(TraverseList, vectors)
{
    irkit::traverse_list(docs, scores, acc, 2);
    EXPECT_THAT(acc, ::testing::ElementsAreArray({0, 2, 4, 6}));
}

TEST_F(TraverseList, vectors_no_weight)
{
    irkit::traverse_list(docs, scores, acc);
    EXPECT_THAT(acc, ::testing::ElementsAreArray({0, 1, 2, 3}));
}

TEST_F(TraverseList, spans)
{
    gsl::span<int> docspan(docs);
    gsl::span<int> scorespan(scores);
    irkit::traverse_list(docspan, scorespan, acc, 1);
    EXPECT_THAT(acc, ::testing::ElementsAreArray({0, 1, 2, 3}));
}

//TEST_F(TraverseList, hash_table_accumulators)
//{
//    std::unordered_map<int, int> acc;
//    irkit::traverse_list(docs, scores, acc, 1);
//    EXPECT_EQ(acc[0], 0);
//    EXPECT_EQ(acc[1], 1);
//    EXPECT_EQ(acc[2], 2);
//    EXPECT_EQ(acc[3], 3);
//}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
