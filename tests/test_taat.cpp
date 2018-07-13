#include <vector>
#include <unordered_map>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gsl/span"

#include "irkit/taat.hpp"

namespace {

class taat : public ::testing::Test {
protected:
    std::vector<int> docs = {1, 2, 3};
    std::vector<int> scores = {1, 2, 3};
    std::vector<int> acc = {0, 0, 0, 0};
};

TEST_F(taat, vectors)
{
    irk::accumulate(docs, scores, acc);
    EXPECT_THAT(acc, ::testing::ElementsAreArray({0, 1, 2, 3}));
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
