#include <unordered_map>
#include <vector>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#define private public
#define protected public
#include "irkit/index.hpp"
#include "irkit/index/postingrange.hpp"
#include "irkit/score.hpp"

namespace {

using Posting = irk::_posting<int, double>;

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
    irk::
        dynamically_scored_posting_range<Posting, int, irk::score::count_scorer>
            range(std::move(docvec),
                std::move(countvec),
                docvec.size(),
                10,
                irk::score::count_scorer{});
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
