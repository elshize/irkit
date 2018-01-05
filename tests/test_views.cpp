#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "irkit/heap.hpp"
#include "irkit/view.hpp"

namespace {

using namespace irkit::view;
using posting = std::pair<int, double>;

bool equal_to(const posting& lhs, const posting& rhs)
{
    return lhs.first == rhs.first;
}

posting add(const posting& lhs, const posting& rhs)
{
    assert(lhs.first == rhs.first);
    return {lhs.first, lhs.second + rhs.second};
}

bool greater(const posting& lhs, const posting& rhs)
{
    return lhs.second > rhs.second;
}

posting multiply_posting(posting p, double weight)
{
    return posting{p.first, p.second * weight};
}

TEST(union_merge_view, ints)
{
    std::vector<std::vector<int>> list_of_lists = {{0, 1, 2}, {0, 2}, {1}, {}};
    std::vector<int> postings =
        union_merge_view(ranges::view::all(list_of_lists)) | ranges::to_vector;
    std::vector<int> expected = {0, 0, 1, 1, 2, 2};
    EXPECT_THAT(postings, ::testing::ElementsAreArray(expected));
}

TEST(union_merge_view, postings)
{
    std::vector<std::vector<posting>> list_of_lists = {
        {{0, 3.0}, {1, 3.0}, {2, 3.0}}, {{0, 3.0}, {2, 3.0}}, {{1, 3.0}}};
    std::vector<posting> postings =
        union_merge_view(ranges::view::all(list_of_lists)) | ranges::to_vector;
    std::vector<posting> expected = {
        {0, 3.0}, {0, 3.0}, {1, 3.0}, {1, 3.0}, {2, 3.0}, {2, 3.0}};
    EXPECT_THAT(postings, ::testing::ElementsAreArray(expected));
}

TEST(group_sorted, ints)
{
    std::vector<int> postings = {0, 0, 1, 2, 2, 2};
    auto grouped = postings | group_sorted();
    std::vector<std::vector<int>> expected = {{0, 0}, {1}, {2, 2, 2}};
    for (auto[actual_group, exp_group] : ranges::view::zip(grouped, expected)) {
        EXPECT_THAT(actual_group | ranges::to_vector,
            ::testing::ElementsAreArray(exp_group));
    }
}

 TEST(group_sorted, postings)
{
    std::vector<posting> postings = {
        {0, 2.0}, {0, 3.0}, {1, 3.0}, {2, 1.0}, {2, 2.0}, {2, 3.0}};
    auto grouped = postings | group_sorted(equal_to);
    std::vector<std::vector<posting>> expected = {
        {{0, 2.0}, {0, 3.0}}, {{1, 3.0}}, {{2, 1.0}, {2, 2.0}, {2, 3.0}}};
    for (auto[actual_group, exp_group] : ranges::view::zip(grouped, expected))
    {
        EXPECT_THAT(actual_group | ranges::to_vector,
            ::testing::ElementsAreArray(exp_group));
    }
}

TEST(accumulate_groups, ints)
{
    std::vector<std::vector<int>> groups = {{0, 0}, {1}, {2, 2, 2}};
    auto accumulated = groups | accumulate_groups();
    std::vector<int> expected = {0, 1, 6};
    EXPECT_THAT(
        accumulated | ranges::to_vector, ::testing::ElementsAreArray(expected));
}

TEST(accumulate_groups, postings)
{
    std::vector<std::vector<posting>> groups = {
        {{0, 2.0}, {0, 3.0}}, {{1, 3.0}}, {{2, 1.0}, {2, 2.0}, {2, 3}}};
    auto accumulated = groups | accumulate_groups(add);
    std::vector<posting> expected = {{0, 5.0}, {1, 3.0}, {2, 6.0}};
    EXPECT_THAT(
        accumulated | ranges::to_vector, ::testing::ElementsAreArray(expected));
}

TEST(accumulate_sorted, ints)
{
    std::vector<int> postings = {0, 0, 1, 2, 2, 2};
    auto accumulated = postings | accumulate_sorted();
    std::vector<int> expected = {0, 1, 6};
    EXPECT_THAT(
        accumulated | ranges::to_vector, ::testing::ElementsAreArray(expected));
}

TEST(accumulate_sorted, postings)
{
    std::vector<posting> postings = {
        {0, 2.0}, {0, 3.0}, {1, 3.0}, {2, 1.0}, {2, 2.0}, {2, 3.0}};
    auto accumulated = postings | accumulate_sorted(equal_to, add);
    std::vector<posting> expected = {{0, 5.0}, {1, 3.0}, {2, 6.0}};
    EXPECT_THAT(
        accumulated | ranges::to_vector, ::testing::ElementsAreArray(expected));
}

TEST(top_k, ints)
{
    std::vector<int> postings = {3, 2, 1, 3, 4, 7, 7};
    auto top = postings | top_k(4);
    std::vector<int> expected = {7, 7, 4, 3};
    EXPECT_THAT(top, ::testing::ElementsAreArray(expected));
}

TEST(top_k, postings)
{
    std::vector<posting> postings = {{0, 5}, {1, 3}, {2, 6}, {3, 2}, {4, 3}};
    auto top = postings | top_k(3, greater);
    std::vector<posting> expected = {{2, 6.0}, {0, 5.0}, {1, 3.0}};
    EXPECT_THAT(top, ::testing::ElementsAreArray(expected));
}

TEST(weighted, ints)
{
    std::vector<int> rng = {0, 1, 2, 3, 4, 5};
    auto wrng = rng | weighted(2) | ranges::to_vector;
    std::vector<int> expected = {0, 2, 4, 6, 8, 10};
    EXPECT_THAT(wrng, ::testing::ElementsAreArray(expected));
}

TEST(weighted, postings)
{
    std::vector<posting> rng = {{0, 0}, {1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}};
    auto wrng = rng | weighted(2.0, multiply_posting) | ranges::to_vector;
    std::vector<posting> expected = {
        {0, 0}, {1, 2}, {2, 4}, {3, 6}, {4, 8}, {5, 10}};
    EXPECT_THAT(wrng, ::testing::ElementsAreArray(expected));
}

TEST(disjunctive_daat, postings)
{
    std::vector<std::vector<posting>> postings = {
        {{0, 3.0}, {1, 3.0}, {2, 3.0}}, {{0, 3.0}, {2, 3.0}}, {{1, 3.0}}};
    std::vector<double> weights = {1.0, 2.0, 3.0};
    auto range_daat_results = ranges::view::zip(postings, weights)
        | ranges::view::transform([=](auto pw) {
              return pw.first
                  | irkit::view::weighted(pw.second, multiply_posting);
          })
        | irkit::view::union_merge()
        | irkit::view::accumulate_sorted(equal_to, add)
        | irkit::view::top_k(1, greater)
        | ranges::to_vector;
    std::vector<posting> expected = {{1, 12.0}};
    EXPECT_THAT(range_daat_results, ::testing::ElementsAreArray(expected));
}

TEST(any_range, vector)
{
    std::vector<int> v = {1, 2, 3};
    auto rng = irkit::view::any_range(v);
    std::vector<int> actual = irkit::view::to_vector(rng);
    std::vector<int> expected  = {1, 2, 3};
    EXPECT_THAT(actual, ::testing::ElementsAreArray(expected));
}

TEST(any_range, transform)
{
    std::vector<std::pair<int, int>> v = {{1, 2}, {2, 3}, {3, 4}, {4, 5}};
    auto view = irkit::view::any_range(irkit::view::transform_view(
        v, [](auto& val) { return val.first + val.second; }));
    std::vector<int> actual = irkit::view::to_vector(view);
    std::vector<int> expected = {3, 5, 7, 9};
    EXPECT_THAT(actual, ::testing::ElementsAreArray(expected));
}

//TEST(take_while, empty_source)
//{
//    std::vector<int> v = {};
//    auto rng = irkit::view::take_while_view(v, [](auto& x) { return x == 1; });
//    std::vector<int> actual = irkit::view::to_vector(rng);
//    std::vector<int> expected  = {};
//    EXPECT_THAT(actual, ::testing::ElementsAreArray(expected));
//}
//
//TEST(take_while, empty_target)
//{
//    std::vector<int> v = {1, 1, 1, 2, 2, 3, 3};
//    auto rng = irkit::view::take_while_view(v, [](auto& x) { return x == 2; });
//    std::vector<int> actual = irkit::view::to_vector(rng);
//    std::vector<int> expected  = {};
//    EXPECT_THAT(actual, ::testing::ElementsAreArray(expected));
//}
//
//TEST(take_while, ints)
//{
//    std::vector<int> v = {1, 1, 1, 2, 2, 3, 3};
//    auto rng = irkit::view::take_while_view(v, [](auto& x) { return x == 1; });
//    std::vector<int> actual = irkit::view::to_vector(rng);
//    std::vector<int> expected  = {1, 1, 1};
//    EXPECT_THAT(actual, ::testing::ElementsAreArray(expected));
//    auto tail = rng.tail();
//}

TEST(fast_transform, empty)
{
    std::vector<int> v = {};
    irkit::view::transform_view view(v, [](int val) { return val + 1; });
    std::vector<int> actual = irkit::view::to_vector(view);
    EXPECT_EQ(actual.size(), 0u);
}

TEST(fast_transform, same_type)
{
    std::vector<int> v = {1, 2, 3, 4};
    irkit::view::transform_view view(v, [](int val) { return val + 1; });
    std::vector<int> actual = irkit::view::to_vector(view);
    std::vector<int> expected = {2, 3, 4, 5};
    EXPECT_THAT(actual, ::testing::ElementsAreArray(expected));
}

TEST(fast_transform, pairs)
{
    std::vector<std::pair<int, int>> v = {{1, 2}, {2, 3}, {3, 4}, {4, 5}};
    irkit::view::transform_view view(v, [](auto& val) { return val.first + val.second; });
    std::vector<int> actual = irkit::view::to_vector(view);
    std::vector<int> expected = {3, 5, 7, 9};
    EXPECT_THAT(actual, ::testing::ElementsAreArray(expected));
}

TEST(aggregate_sorted_view, ints)
{
    std::vector<int> v = {1, 1, 1, 2, 2, 3, 3, 4, 4};
    irkit::view::aggregate_sorted_view view(
        v, std::equal_to<int>{}, std::plus<int>{});
    std::vector<int> actual = irkit::view::to_vector(view);
    std::vector<int> expected = {3, 4, 6, 8};
    EXPECT_THAT(actual, ::testing::ElementsAreArray(expected));
}

TEST(aggregate_sorted_view, pairs)
{
    std::vector<posting> v = {
        {1, 1}, {1, 2}, {1, 3}, {2, 1}, {2, 3}, {3, 2}, {3, 4}, {4, 2}, {4, 6}};
    irkit::view::aggregate_sorted_view view(v, equal_to, add);
    std::vector<posting> actual = irkit::view::to_vector(view);
    std::vector<posting> expected = {{1, 6}, {2, 4}, {3, 6}, {4, 8}};
    EXPECT_THAT(actual, ::testing::ElementsAreArray(expected));
}

TEST(aggregate_sorted_view, every_other_group)
{
    std::vector<posting> v = {
        {1, 1}, {1, 2}, {1, 3}, {2, 1}, {2, 3}, {3, 2}, {3, 4}, {4, 2}, {4, 6}};
    irkit::view::aggregate_sorted_view view(v, equal_to, add);
    std::vector<posting> actual;
    std::vector<posting> actual2;
    bool take = true;
    for (auto it = view.begin(); it != view.end(); it++) {
        if (take) {
            actual.push_back(*it);
            actual2.push_back(*it);
            take = false;
        } else {
            take = true;
        }
    }
    std::vector<posting> expected = {{1, 6}, {3, 6}};
    EXPECT_THAT(actual, ::testing::ElementsAreArray(expected));
    EXPECT_THAT(actual2, ::testing::ElementsAreArray(expected));

    actual.clear();
    actual2.clear();
    take = false;
    for (auto it = view.begin(); it != view.end(); ++it) {
        if (take) {
            actual.push_back(*it);
            actual2.push_back(*it);
            take = false;
        } else {
            take = true;
        }
    }
    expected = {{2, 4}, {4, 8}};
    EXPECT_THAT(actual, ::testing::ElementsAreArray(expected));
    EXPECT_THAT(actual2, ::testing::ElementsAreArray(expected));
}

TEST(fast_top_k, ints)
{
    std::vector<int> postings = {3, 2, 1, 3, 4, 7, 7};
    auto top = top_k(postings, 4);
    std::vector<int> expected = {7, 7, 4, 3};
    EXPECT_THAT(top, ::testing::ElementsAreArray(expected));
}

TEST(fast_top_k, postings)
{
    std::vector<posting> postings = {{0, 5}, {1, 3}, {2, 6}, {3, 2}, {4, 3}};
    auto top = top_k(postings, 3, greater);
    std::vector<posting> expected = {{2, 6.0}, {0, 5.0}, {1, 3.0}};
    EXPECT_THAT(top, ::testing::ElementsAreArray(expected));
}

TEST(fast_union_merge_view, ints)
{
    std::vector<std::vector<int>> list_of_lists = {{0, 1, 2}, {0, 2}, {1}, {}};
    auto postings = irkit::view::fast_union_merge_view(list_of_lists);
    std::vector<int> actual = irkit::view::to_vector(postings);
    std::vector<int> expected = {0, 0, 1, 1, 2, 2};
    EXPECT_THAT(actual, ::testing::ElementsAreArray(expected));
}

TEST(fast_union_merge_view, postings)
{
    std::vector<std::vector<posting>> list_of_lists = {
        {{0, 3.0}, {1, 3.0}, {2, 3.0}}, {{0, 3.0}, {2, 3.0}}, {{1, 3.0}}};
    auto postings = irkit::view::fast_union_merge_view(list_of_lists);
    std::vector<posting> actual = irkit::view::to_vector(postings);
    std::vector<posting> expected = {
        {0, 3.0}, {0, 3.0}, {1, 3.0}, {1, 3.0}, {2, 3.0}, {2, 3.0}};
    EXPECT_THAT(actual, ::testing::ElementsAreArray(expected));
}

TEST(fast_disjunctive_daat, postings)
{
    std::vector<std::vector<posting>> postings = {
        {{0, 3.0}, {1, 3.0}, {2, 3.0}}, {{0, 3.0}, {2, 3.0}}, {{1, 3.0}}};
    std::vector<double> weights = {1.0, 2.0, 3.0};
    std::size_t idx = 0;
    auto weighted_postlists =
        irkit::view::to_vector(irkit::view::transform_view(
            postings, [&weights, &idx](auto& postlist) {
                auto weight = weights[idx++];
                return irkit::view::transform_view(
                    postlist, [weight](auto& post) {
                        return multiply_posting(post, weight);
                    });
            }));
    auto union_view = irkit::view::fast_union_merge_view(weighted_postlists);
    auto aggregated =
        irkit::view::aggregate_sorted_view(union_view, equal_to, add);
    auto top = irkit::view::top_k(aggregated, 1, greater);
    std::vector<posting> actual = irkit::view::to_vector(top);
    std::vector<posting> expected = {{1, 12.0}};
    EXPECT_THAT(actual, ::testing::ElementsAreArray(expected));
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

