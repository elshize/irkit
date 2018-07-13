#include <ostream>
#include <vector>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#define private public
#define protected public
#include "irkit/daat.hpp"

//struct posting {
//    int doc;
//    double score;
//};
//
//posting operator+(const posting& lhs, const posting& rhs)
//{
//    assert(lhs.doc == rhs.doc);
//    return posting{lhs.doc, lhs.score + rhs.score};
//}
//
//posting operator*(const posting& p, const double& weight)
//{
//    return posting{p.doc, p.score * weight};
//}
//
//bool operator==(const posting& lhs, const posting& rhs)
//{
//    return lhs.doc == rhs.doc;
//}
//
//bool operator>(const posting& lhs, const posting& rhs)
//{
//    return lhs.doc > rhs.doc;
//}
//
//std::ostream& operator<<(std::ostream& os, posting p)
//{
//    return os << "{" << p.doc << "," << p.score << "}";
//}
//
//namespace {
//
//class DaatTestPair : public ::testing::Test {
//protected:
//    std::vector<std::vector<posting>> postings;
//
//    virtual void SetUp()
//    {
//        postings = {{{0, 1}, {1, 3}, {2, 1}}, {{0, 1}, {2, 3}}, {{1, 5}}};
//    }
//};
//
//TEST_F(DaatTestPair, moving_range)
//{
//    std::vector<posting> p = postings[0];
//    auto mr = irk::moving_range(p.begin(), p.end());
//    EXPECT_EQ(mr.empty(), false);
//    EXPECT_EQ(mr.begin()->doc, 0);
//    EXPECT_EQ(mr.begin()->score, 1);
//    mr.advance();
//    EXPECT_EQ(mr.empty(), false);
//    EXPECT_EQ(mr.begin()->doc, 1);
//    EXPECT_EQ(mr.begin()->score, 3);
//    mr.advance();
//    EXPECT_EQ(mr.empty(), false);
//    EXPECT_EQ(mr.begin()->doc, 2);
//    EXPECT_EQ(mr.begin()->score, 1);
//    mr.advance();
//    EXPECT_EQ(mr.empty(), true);
//}
//
//TEST_F(DaatTestPair, union_next_posting)
//{
//    std::vector<double> weights = {3, 3, 3};
//    irk::union_range posting_union(postings, weights);
//    std::vector<irk::union_range<std::vector<posting>>::docterm> expected_heap;
//
//    posting p = posting_union.next_posting();
//    ASSERT_EQ(p.doc, 0);
//    ASSERT_EQ(p.score, 3);
//    expected_heap = {{0, 0}, {2, 1}, {1, 2}};
//    ASSERT_THAT(
//        posting_union.heap_, ::testing::ElementsAreArray(expected_heap));
//
//    p = posting_union.next_posting();
//    ASSERT_EQ(p.doc, 0);
//    ASSERT_EQ(p.score, 3);
//    expected_heap = {{1, 2}, {2, 1}, {1, 0}};
//    ASSERT_THAT(
//        posting_union.heap_, ::testing::ElementsAreArray(expected_heap));
//
//    p = posting_union.next_posting();
//    ASSERT_EQ(p.doc, 1);
//    ASSERT_EQ(p.score, 15);
//    expected_heap = {{1, 0}, {2, 1}};
//    ASSERT_THAT(
//        posting_union.heap_, ::testing::ElementsAreArray(expected_heap));
//
//    p = posting_union.next_posting();
//    ASSERT_EQ(p.doc, 1);
//    ASSERT_EQ(p.score, 9);
//    expected_heap = {{2, 1}, {2, 0}};
//    ASSERT_THAT(
//        posting_union.heap_, ::testing::ElementsAreArray(expected_heap));
//
//    p = posting_union.next_posting();
//    ASSERT_EQ(p.doc, 2);
//    ASSERT_EQ(p.score, 9);
//    expected_heap = {{2, 0}};
//    ASSERT_THAT(
//        posting_union.heap_, ::testing::ElementsAreArray(expected_heap));
//
//    p = posting_union.next_posting();
//    ASSERT_EQ(p.doc, 2);
//    ASSERT_EQ(p.score, 3);
//    expected_heap = {};
//    ASSERT_THAT(
//        posting_union.heap_, ::testing::ElementsAreArray(expected_heap));
//}
//
//TEST(daat_or, postings_weights)
//{
//    std::vector<std::vector<posting>> postings = {
//        {{0, 1}, {1, 3}, {2, 1}}, {{0, 1}, {2, 3}}, {{1, 5}}};
//    std::vector<double> weights = {3, 3, 3};
//    std::vector<posting> top = irk::daat_or(postings, 2, weights);
//    std::vector<posting> expected = {{1, 24}, {2, 12}};
//    for (std::size_t idx = 0; idx < expected.size(); idx++) {
//        EXPECT_EQ(top[idx].doc, expected[idx].doc);
//        EXPECT_EQ(top[idx].score, expected[idx].score);
//    }
//}
//
//};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
