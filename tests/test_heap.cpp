#include <algorithm>
#include <experimental/filesystem>
#include <iostream>
#include <vector>
#include <unordered_map>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#define private public
#include "heap.hpp"

namespace {

using namespace irk;

class HeapTest : public ::testing::Test {
protected:
    Heap<int, char> heap;
};

TEST_F(HeapTest, empty)
{
    EXPECT_EQ(heap.size(), 0);
    EXPECT_EQ(heap.empty(), true);
}

TEST_F(HeapTest, from_nonempty)
{
    EXPECT_EQ(heap.size(), 0);

    heap.push(5, 'a');
    EXPECT_EQ(heap.size(), 1);
    EXPECT_EQ(heap.top(), make_entry(5, 'a'));

    heap.push(3, 'b');
    EXPECT_EQ(heap.size(), 2);
    EXPECT_EQ(heap.top(), make_entry(3, 'b'));

    heap.push(1, 'c');
    EXPECT_EQ(heap.size(), 3);
    EXPECT_EQ(heap.top(), make_entry(1, 'c'));

    heap.push(2, 'd');
    EXPECT_EQ(heap.size(), 4);
    EXPECT_EQ(heap.top(), make_entry(1, 'c'));

    auto entry = heap.pop_push(6, 'e');
    EXPECT_EQ(heap.size(), 4);
    EXPECT_EQ(entry, make_entry(1, 'c'));
    EXPECT_EQ(heap.top(), make_entry(2, 'd'));

    std::vector<Entry<int, char>> v(heap.begin(), heap.end());
    std::sort(v.begin(), v.end());
    std::vector<Entry<int, char>> expv = {
        {2, 'd'}, {3, 'b'}, {5, 'a'}, {6, 'e'}};
    EXPECT_THAT(v, ::testing::ElementsAreArray(expv));

    heap.push_with_limit(1, 'f', 4);
    EXPECT_EQ(heap.size(), 4);
    EXPECT_EQ(heap.top(), make_entry(2, 'd'));

    heap.push_with_limit(2, 'g', 4);
    EXPECT_EQ(heap.size(), 4);
    EXPECT_EQ(heap.top(), make_entry(2, 'g'));

    EXPECT_EQ(heap.pop(), make_entry(2, 'g'));
    EXPECT_EQ(heap.pop(), make_entry(3, 'b'));
    EXPECT_EQ(heap.pop(), make_entry(5, 'a'));
    EXPECT_EQ(heap.pop(), make_entry(6, 'e'));
}

TEST(MappingTest, regular_operations)
{
    using HeapType = Heap<int, char, std::less<int>, std::unordered_map<int, int>>;
    HeapType heap;
    EXPECT_EQ(heap.mapping.size(), 0);

    heap.push(5, 'a');
    heap.push(3, 'b');
    heap.push(1, 'c');
    heap.push(2, 'd');
    heap.pop_push(6, 'e');

    std::vector<Entry<int, char>> v(heap.begin(), heap.end());
    std::sort(v.begin(), v.end());
    std::vector<Entry<int, char>> expv = {
        {2, 'd'}, {3, 'b'}, {5, 'a'}, {6, 'e'}};
    EXPECT_THAT(v, ::testing::ElementsAreArray(expv));
}

TEST(MappingTest, remove)
{
    using HeapType = Heap<int, char, std::less<int>, std::unordered_map<char, int>>;
    HeapType heap;
    EXPECT_EQ(heap.mapping.size(), 0);

    heap.push(1, 'a');
    heap.push(4, 'b');
    heap.push(2, 'c');
    heap.push(5, 'd');
    heap.push(6, 'e');
    heap.push(7, 'f');
    heap.push(3, 'g');
    {
        std::vector<Entry<int, char>> v(heap.begin(), heap.end());
        std::vector<Entry<int, char>> expv = {{1, 'a'},
            {4, 'b'},
            {2, 'c'},
            {5, 'd'},
            {6, 'e'},
            {7, 'f'},
            {3, 'g'}};
        EXPECT_THAT(v, ::testing::ElementsAreArray(expv));
    }

    heap.remove_value('e');
    {
        std::vector<Entry<int, char>> v(heap.begin(), heap.end());
        std::vector<Entry<int, char>> expv = {{1, 'a'},
            {3, 'g'},
            {2, 'c'},
            {5, 'd'},
            {4, 'b'},
            {7, 'f'}
        };
        EXPECT_THAT(v, ::testing::ElementsAreArray(expv));
    }

    ASSERT_THROW(heap.pop_push(2, 'c'), std::invalid_argument);

    heap.push(8, 'c');
    {
        std::vector<Entry<int, char>> v(heap.begin(), heap.end());
        std::vector<Entry<int, char>> expv = {{1, 'a'},
            {3, 'g'},
            {7, 'f'},
            {5, 'd'},
            {4, 'b'},
            {8, 'c'}
        };
        EXPECT_THAT(v, ::testing::ElementsAreArray(expv));
    }

}

}  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
