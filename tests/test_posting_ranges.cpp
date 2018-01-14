#include <unordered_map>
#include <vector>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#define private public
#define protected public
#include "irkit/index.hpp"

namespace {

using Posting = irkit::_Posting<int, double>;

struct FakeScore {
    template<class Freq>
    double operator()(Freq tf, Freq df, std::size_t collection_size) const
    {
        return tf;
    }
};

TEST(DynamiclyScoredPostingRange, iterator)
{
    std::vector<int> docvec = {0, 1, 5};
    std::vector<int> countvec = {4, 10, 2};
    irkit::DynamiclyScoredPostingRange<Posting, int, FakeScore> range(
        std::move(docvec), std::move(countvec), docvec.size(), 10, FakeScore{});
    std::vector<Posting> expected = {{0, 4}, {1, 10}, {5, 2}};
    std::vector<Posting> actual;
    for (const auto& posting : range) {
        actual.push_back(posting);
    }
    EXPECT_THAT(actual, ::testing::ElementsAreArray(expected));
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
