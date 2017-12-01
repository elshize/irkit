#include <algorithm>
#include <experimental/filesystem>
#include <iostream>
#include <vector>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "heap.hpp"

namespace {

using namespace bloodhound;

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

    std::vector<Heap<int, char>::Entry> v(heap.begin(), heap.end());
    std::sort(v.begin(), v.end());
    std::vector<Heap<int, char>::Entry> expv = {
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

}  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
