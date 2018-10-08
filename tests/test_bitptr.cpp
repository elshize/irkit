#include <bitset>
#include <string>
#include <vector>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public
#include <irkit/bitptr.hpp>

namespace {

class bitptr : public ::testing::Test {
protected:
    std::string s = {0b01010101, (char)0b11111111};
    std::bitset<16> expected = std::bitset<16>("1111111101010101");
};

TEST_F(bitptr, construct)
{
    for (std::size_t idx = 0; idx < 16; ++idx) {
        irk::bitptr bp(s.data(), idx);
        ASSERT_EQ(*bp, expected[idx]) << "index " << idx << " of " << expected;
    }
}

TEST_F(bitptr, increment)
{
    irk::bitptr bp(s.data());
    for (std::size_t idx = 0; idx < 16; ++idx) {
        ASSERT_EQ(*bp, expected[idx]) << "index " << idx << " of " << expected
                                      << "\npointer: " << (int)bp.shift_;
        ++bp;
    }
}

TEST_F(bitptr, indexing)
{
    irk::bitptr bp(s.data());
    for (std::size_t idx = 0; idx < 16; ++idx) {
        ASSERT_EQ(bp[idx], expected[idx])
            << "index " << idx << " of " << expected;
    }
}

TEST_F(bitptr, add)
{
    irk::bitptr bp(s.data());
    for (std::size_t idx = 0; idx < 16; ++idx) {
        irk::bitptr bpa = bp + idx;
        ASSERT_EQ(*bpa, expected[idx]) << "index " << idx << " of " << expected;
    }
}

TEST_F(bitptr, set_by_reference)
{
    irk::bitptr bp(s.data());
    std::string s = {(char)0b11111111, (char)0b11111111};
    for (std::size_t idx = 0; idx < 16; ++idx) {
        *bp = 1;
    }
}

TEST_F(bitptr, set_by_index_operator)
{
    irk::bitptr bp(s.data());
    std::string s = {(char)0b11111111, (char)0b11111111};
    for (std::size_t idx = 0; idx < 16; ++idx) {
        bp[idx] = 1;
    }
}

class bitcpy : public ::testing::Test {
protected:
    std::string source = {0b00000000, (char)0b11111111};
    std::string target = {(char)0b11111111, 0b00000000};
};

TEST_F(bitcpy, all)
{
    irk::bitcpy(irk::bitptr(target.data()), irk::bitptr(source.data()), 16);
    ASSERT_EQ(target, source);
}

TEST_F(bitcpy, middle)
{
    irk::bitcpy(
        irk::bitptr(target.data(), 4), irk::bitptr(source.data(), 4), 8);
    std::string expected = {(char)0b00001111, 0b00001111};
    ASSERT_EQ(target, expected);
}

TEST_F(bitcpy, bitset)
{
    irk::bitcpy(
        irk::bitptr(target.data(), 4), boost::dynamic_bitset<>(8, 0b11110000));
    std::string expected = {(char)0b00001111, 0b00001111};
    ASSERT_EQ(target, expected);
}

}  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

