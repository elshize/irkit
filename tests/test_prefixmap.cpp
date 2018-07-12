#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public
#include <irkit/alphabetical_bst.hpp>
#include <irkit/coding/hutucker.hpp>
#include <irkit/prefixmap.hpp>
#include <irkit/radix_tree.hpp>

namespace {

namespace fs = boost::filesystem;

class abst_test : public ::testing::Test {
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
    std::shared_ptr<irk::hutucker_codec<>> ht()
    {
        std::vector<char> mem;
        for (auto& node : nodes) {
            mem.insert(mem.end(),
                std::begin(node.bytes),
                std::next(std::begin(node.bytes), 5));
        }
        return std::make_shared<irk::hutucker_codec<>>(
            irk::alphabetical_bst(mem));
    }
};

class block_builder_test : public abst_test {
};

class prefix_map_test : public abst_test {
};

using map_type = irk::prefix_map<int, std::vector<char>>;
using block_builder = irk::prefix_map<int, std::vector<char>>::block_builder;
using block_ptr = irk::prefix_map<int, std::vector<char>>::block_ptr;

TEST_F(block_builder_test, fits)
{
    std::vector<char> block_mem(1024);
    auto codec = ht();
    block_builder b(1, block_mem.data(), block_mem.size(), codec);
    b.add("aaa");
    b.add("aab");
    b.add("aabbbb");
    b.add("b");
    EXPECT_EQ(b.count_, 4);
    b.write_count();
    EXPECT_EQ(b.first_index_, 1);

    block_ptr p(block_mem.data(), codec);
    EXPECT_EQ(p.first_index(), 1);
    EXPECT_EQ(p.count(), 4);
    EXPECT_EQ(p.next(), "aaa");
    EXPECT_EQ(p.next(), "aab");
    EXPECT_EQ(p.next(), "aabbbb");
    EXPECT_EQ(p.next(), "b");
}

TEST_F(block_builder_test, does_not_fit)
{
    std::vector<char> block_mem(15);
    auto codec = ht();
    block_builder b(1, block_mem.data(), block_mem.size(), codec);
    ASSERT_EQ(b.add("aaa"), true);
    ASSERT_EQ(b.add("aab"), true);
    ASSERT_EQ(b.add("aabbbb"), true);
    ASSERT_EQ(b.add("b"), false);
    ASSERT_EQ(b.count_, 3);
    b.write_count();
    EXPECT_EQ(b.first_index_, 1);

    block_ptr p(block_mem.data(), codec);
    EXPECT_EQ(p.first_index(), 1);
    EXPECT_EQ(p.count(), 3);
    EXPECT_EQ(p.next(), "aaa");
    EXPECT_EQ(p.next(), "aab");
    EXPECT_EQ(p.next(), "aabbbb");
}

TEST_F(prefix_map_test, from_strings)
{
    std::vector<std::string> strings = {"aaa", "aab", "aabbbb", "b"};
    auto codec = ht();
    irk::prefix_map<int, std::vector<char>> map(strings, codec);
    EXPECT_EQ(map["aaa"].value(), 0);
    EXPECT_EQ(map["aab"].value(), 1);
    EXPECT_EQ(map["aabbbb"].value(), 2);
    EXPECT_EQ(map["b"].value(), 3);
    EXPECT_EQ(map["aaba"], std::nullopt);
    EXPECT_EQ(map["baaa"], std::nullopt);
}

TEST_F(prefix_map_test, multiple_blocks)
{
    std::vector<std::string> strings = {"aaa", "aab", "aabbbb", "b"};
    auto codec = ht();
    irk::prefix_map<int, std::vector<char>> map(strings, codec, 15);
    EXPECT_EQ(map["aaa"].value(), 0);
    EXPECT_EQ(map["aab"].value(), 1);
    EXPECT_EQ(map["aabbbb"].value(), 2);
    EXPECT_EQ(map["b"].value(), 3);
    EXPECT_EQ(map["aaba"], std::nullopt);
    EXPECT_EQ(map["baaa"], std::nullopt);
}

TEST(build_prefix_map, from_strings)
{
    std::vector<std::string> strings = {"aaa", "aab", "aabbbb", "b"};
    auto map = irk::build_prefix_map<int>(strings);
    EXPECT_EQ(map.block_count_, 1);
    EXPECT_EQ(map["aaa"].value(), 0);
    EXPECT_EQ(map["aab"].value(), 1);
    EXPECT_EQ(map["aabbbb"].value(), 2);
    EXPECT_EQ(map["b"].value(), 3);
    EXPECT_EQ(map["aaba"], std::nullopt);
    EXPECT_EQ(map["baaa"], std::nullopt);
}

TEST(build_prefix_map, expand_block)
{
    std::vector<std::string> strings = {"aaa", "aab", "aabbbbbbbbbbbb", "b"};
    auto map = irk::build_prefix_map<int>(strings, 10);
    EXPECT_EQ(map.block_count_, 4);
    EXPECT_EQ(map["aaa"].value(), 0);
    EXPECT_EQ(map["aab"].value(), 1);
    EXPECT_EQ(map["aabbbbbbbbbbbb"].value(), 2);
    EXPECT_EQ(map["b"].value(), 3);
    EXPECT_EQ(map["aaba"], std::nullopt);
    EXPECT_EQ(map["baaa"], std::nullopt);
}

// TODO: both "a" and "aa" in leaders -- maybe test in mbt instead?

TEST(build_prefix_map, lorem)
{
    fs::path in_file("randstr.txt");
    std::ifstream in(in_file.c_str());
    std::vector<std::string> strings;
    std::string line;
    while (std::getline(in, line)) {
        strings.push_back(line);
    }
    in.close();
    std::sort(strings.begin(), strings.end());
    auto map = irk::build_prefix_map<int>(strings);
    for (std::size_t idx = 0; idx < strings.size(); ++idx) {
        auto key = strings[idx];
        auto retrieved_idx = map[key];
        ASSERT_NE(retrieved_idx, std::nullopt) << key;
        ASSERT_EQ(retrieved_idx.value(), idx);
    }
}

TEST(build_prefix_map, lorem_multiple_blocks)
{
    fs::path in_file("randstr.txt");
    std::ifstream in(in_file.c_str());
    std::vector<std::string> strings;
    std::string line;
    while (std::getline(in, line)) {
        strings.push_back(line);
    }
    in.close();
    std::sort(strings.begin(), strings.end());
    auto map = irk::build_prefix_map<int>(strings, 128);
    for (std::size_t idx = 0; idx < strings.size(); ++idx) {
        auto key = strings[idx];
        auto retrieved_idx = map[key];
        ASSERT_NE(retrieved_idx, std::nullopt) << key << " (" << idx << ")";
        ASSERT_EQ(retrieved_idx.value(), idx) << key << " (" << idx << ")";
    }
}

TEST(dump_and_load_prefix_map, from_strings)
{
    std::vector<std::string> strings = {"aaa", "aab", "aabbbb", "b"};
    auto map = irk::build_prefix_map<int>(strings);
    std::ostringstream out;
    map.dump(out);

    std::istringstream in(out.str());
    auto lmap = irk::load_prefix_map<int>(in);

    EXPECT_EQ(lmap["aaa"].value(), 0);
    EXPECT_EQ(lmap["aab"].value(), 1);
    EXPECT_EQ(lmap["aabbbb"].value(), 2);
    EXPECT_EQ(lmap["b"].value(), 3);
    EXPECT_EQ(lmap["aaba"], std::nullopt);
    EXPECT_EQ(lmap["baaa"], std::nullopt);
}

TEST(dump_and_load_prefix_map, lorem)
{
    fs::path in_file("randstr.txt");
    std::ifstream in(in_file.c_str());
    std::vector<std::string> strings;
    std::string line;
    while (std::getline(in, line)) {
        strings.push_back(line);
    }
    in.close();
    std::sort(strings.begin(), strings.end());
    auto map = irk::build_prefix_map<int>(strings, 128);
    std::ostringstream out;
    map.dump(out);

    std::istringstream inl(out.str());
    auto lmap = irk::load_prefix_map<int>(inl);

    for (std::size_t idx = 0; idx < strings.size(); ++idx) {
        auto key = strings[idx];
        auto retrieved_idx = lmap[key];
        ASSERT_NE(retrieved_idx, std::nullopt) << key;
        ASSERT_EQ(retrieved_idx.value(), idx);
    }
}

TEST(prefix_map, iterator)
{
    fs::path in_file("randstr.txt");
    std::ifstream in(in_file.c_str());
    std::vector<std::string> strings;
    std::string line;
    while (std::getline(in, line)) {
        strings.push_back(line);
    }
    in.close();
    std::sort(strings.begin(), strings.end());
    auto map = irk::build_prefix_map<int>(strings, 128);
    std::ostringstream out;
    map.dump(out);

    std::istringstream inl(out.str());
    auto lmap = irk::load_prefix_map<int>(inl);

    std::vector<std::string> from_map(lmap.begin(), lmap.end());
    for (int idx = 0; idx < 172; idx++)
    { ASSERT_THAT(from_map[idx], ::testing::ElementsAreArray(strings[idx])); }
}

TEST(prefix_map, reverse_lookup)
{
    fs::path in_file("randstr.txt");
    std::ifstream in(in_file.c_str());
    std::vector<std::string> strings;
    std::string line;
    while (std::getline(in, line)) {
        strings.push_back(line);
    }
    in.close();
    std::sort(strings.begin(), strings.end());
    auto map = irk::build_prefix_map<int>(strings, 128);
    std::ostringstream out;
    map.dump(out);

    std::istringstream inl(out.str());
    auto lmap = irk::load_prefix_map<int>(inl);

    for (int idx = 0; idx < 172; idx++)
    { ASSERT_EQ(lmap[lmap[idx]].value_or(-1), idx) << lmap[idx]; }
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

