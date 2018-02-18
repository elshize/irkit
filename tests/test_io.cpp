#include <vector>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#define private public
#define protected public
#include "irkit/alphabetical_bst.hpp"
#include "irkit/bitstream.hpp"
#include "irkit/coding/varbyte.hpp"
#include "irkit/compacttable.hpp"
#include "irkit/io.hpp"

namespace {

namespace fs = std::experimental::filesystem;

TEST(InputBitStream, read)
{
    std::istringstream in(std::string({0b01010101, (char)0b10000000}));
    irk::input_bit_stream ibs(in);
    EXPECT_EQ(ibs.read(), 0);
    EXPECT_EQ(ibs.read(), 1);
    EXPECT_EQ(ibs.read(), 0);
    EXPECT_EQ(ibs.read(), 1);
    EXPECT_EQ(ibs.read(), 0);
    EXPECT_EQ(ibs.read(), 1);
    EXPECT_EQ(ibs.read(), 0);
    EXPECT_EQ(ibs.read(), 1);
    EXPECT_EQ(ibs.read(), 1);
    EXPECT_EQ(ibs.read(), 0);
    EXPECT_EQ(ibs.read(), 0);
    EXPECT_EQ(ibs.read(), 0);
    EXPECT_EQ(ibs.read(), 0);
    EXPECT_EQ(ibs.read(), 0);
    EXPECT_EQ(ibs.read(), 0);
    EXPECT_EQ(ibs.read(), 0);
    EXPECT_EQ(ibs.read(), -1);
}

TEST(OutputBitStream, read)
{
    std::ostringstream out;
    irk::output_bit_stream obs(out);
    obs.write(0);
    obs.write(1);
    obs.write(0);
    obs.write(1);
    obs.write(0);
    obs.write(1);
    obs.write(0);
    obs.write(1);
    obs.write(1);
    obs.write(0);
    obs.write(0);
    obs.write(0);
    obs.write(0);
    obs.write(0);
    obs.write(0);
    obs.write(0);
    obs.flush();
    EXPECT_EQ(out.str(), std::string({0b01010101, (char)0b10000000}));
}

TEST(OffsetTable, from_ints_write_load)
{
    std::vector<std::size_t> offsets = {0, 10, 21, 35, 47, 60};
    irk::offset_table<> offset_table(offsets, 4);
    auto header = offset_table.header();
    EXPECT_EQ(header->count, 6);
    EXPECT_EQ(header->block_size, 4);
    EXPECT_EQ(offset_table.size(), 6);
    std::vector<char> leaders(
        offset_table.data_.begin() + 12, offset_table.data_.begin() + 28);
    std::vector<char> exp_leaders = {
        0, 0, 0, 0, 28, 0, 0, 0, 4, 0, 0, 0, 32, 0, 0, 0};
    EXPECT_THAT(leaders, ::testing::ElementsAreArray(exp_leaders));
    EXPECT_EQ(offset_table[0], 0);
    EXPECT_EQ(offset_table[1], 10);
    EXPECT_EQ(offset_table[2], 21);
    EXPECT_EQ(offset_table[3], 35);
    EXPECT_EQ(offset_table[4], 47);
    EXPECT_EQ(offset_table[5], 60);

    auto offtab_path = irk::io::fs::temp_directory_path() / "offtab";
    irk::io::dump(offset_table, offtab_path);

    offset_table = irk::offset_table<>(offtab_path);
    header = offset_table.header();
    EXPECT_EQ(header->count, 6);
    EXPECT_EQ(header->block_size, 4);
    EXPECT_EQ(offset_table.size(), 6);
    EXPECT_EQ(offset_table[0], 0);
    EXPECT_EQ(offset_table[1], 10);
    EXPECT_EQ(offset_table[2], 21);
    EXPECT_EQ(offset_table[3], 35);
    EXPECT_EQ(offset_table[4], 47);
    EXPECT_EQ(offset_table[5], 60);
}

class CompactABST : public ::testing::Test {
protected:
    const static uint16_t s = 256;
    std::vector<irk::alphabetical_bst<>::node> nodes = {
        {'h', s + 5, 'i'},      // 0
        {'g', s + 10, 'h'},     // 5
        {'f', s + 15, 'g'},     // 10
        {'a', 'a', s + 20},     // 15
        {'c', s + 25, s + 30},  // 20
        {'b', 'b', 'c'},        // 25
        {'e', s + 35, 'f'},     // 30
        {'d', 'd', 'e'}         // 35
    };
    irk::alphabetical_bst<> abst()
    {
        std::vector<char> mem;
        for (auto& node : nodes) {
            mem.insert(mem.end(), node.bytes, node.bytes + 5);
        }
        return irk::alphabetical_bst(mem);
    }
};

boost::dynamic_bitset<unsigned char> bit_vector(std::vector<bool> bool_vector)
{
    boost::dynamic_bitset<unsigned char> bv;
    for (bool bit : bool_vector) {
        bv.push_back(bit);
    }
    return bv;
}

TEST_F(CompactABST, decode)
{
    auto _ = bit_vector;
    char a = abst().decode(_({0, 0, 0, 0}));
    EXPECT_EQ(a, 'a');
    char b = abst().decode(_({0, 0, 0, 1, 0, 0}));
    EXPECT_EQ(b, 'b');
    char c = abst().decode(_({0, 0, 0, 1, 0, 1}));
    EXPECT_EQ(c, 'c');
    char d = abst().decode(_({0, 0, 0, 1, 1, 0, 0}));
    EXPECT_EQ(d, 'd');
    char e = abst().decode(_({0, 0, 0, 1, 1, 0, 1}));
    EXPECT_EQ(e, 'e');
    char f = abst().decode(_({0, 0, 0, 1, 1, 1}));
    EXPECT_EQ(f, 'f');
    char g = abst().decode(_({0, 0, 1}));
    EXPECT_EQ(g, 'g');
    char h = abst().decode(_({0, 1}));
    EXPECT_EQ(h, 'h');
    char i = abst().decode(_({1}));
    EXPECT_EQ(i, 'i');
}

TEST_F(CompactABST, encode)
{
    auto _ = bit_vector;
    auto a = abst().encode('a');
    EXPECT_EQ(a, _({0, 0, 0, 0}));
    auto b = abst().encode('b');
    EXPECT_EQ(b, _({0, 0, 0, 1, 0, 0}));
    auto c = abst().encode('c');
    EXPECT_EQ(c, _({0, 0, 0, 1, 0, 1}));
    auto d = abst().encode('d');
    EXPECT_EQ(d, _({0, 0, 0, 1, 1, 0, 0}));
    auto e = abst().encode('e');
    EXPECT_EQ(e, _({0, 0, 0, 1, 1, 0, 1}));
    auto f = abst().encode('f');
    EXPECT_EQ(f, _({0, 0, 0, 1, 1, 1}));
    auto g = abst().encode('g');
    EXPECT_EQ(g, _({0, 0, 1}));
    auto h = abst().encode('h');
    EXPECT_EQ(h, _({0, 1}));
    auto i = abst().encode('i');
    EXPECT_EQ(i, _({1}));
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

