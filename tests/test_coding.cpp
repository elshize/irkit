#include <algorithm>
#include <random>
#include <sstream>
#include <vector>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#define private public
#define protected public
#include "irkit/coding.hpp"

namespace {

TEST(VarByte, encode)
{
    irkit::VarByte<int> vb;
    std::vector<char> actual = vb.encode({1, 255});
    std::vector<char> expected = {
        (char)0b10000001u, (char)0b01111111, (char)0b10000001};
    EXPECT_THAT(actual, ::testing::ElementsAreArray(expected));
}

TEST(VarByte, encode_fn)
{
    irkit::VarByte<int> vb;
    std::vector<std::pair<int, char>> input = {
        std::make_pair(1, 'a'), std::make_pair(255, 'b')};
    std::vector<char> actual =
        vb.encode(input, [](const auto& p) { return p.first; });
    std::vector<char> expected = {
        (char)0b10000001u, (char)0b01111111, (char)0b10000001};
    EXPECT_THAT(actual, ::testing::ElementsAreArray(expected));
}

TEST(VarByte, decode)
{
    irkit::VarByte<int> vb;
    std::vector<int> actual =
        vb.decode({(char)0b10000001u, (char)0b01111111, (char)0b10000001});
    std::vector<int> expected = {1, 255};
    EXPECT_THAT(actual, ::testing::ElementsAreArray(expected));
}

TEST(VarByte, encode_decode)
{
    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution(0, 1000000);
    auto roll = std::bind(distribution, generator);

    irkit::VarByte<int> vb;

    // given
    std::vector<int> initial(100);
    std::generate(initial.begin(), initial.end(), roll);

    // when
    std::vector<char> encoded = vb.encode(initial);
    std::vector<int> decoded = vb.decode(encoded);

    // then
    EXPECT_THAT(decoded, ::testing::ElementsAreArray(initial));
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}


