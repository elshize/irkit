#include <algorithm>
#include <gsl/span>
#include <random>
#include <sstream>
#include <vector>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#define private public
#define protected public
#include "irkit/bitstream.hpp"
#include "irkit/coding/huffman.hpp"
#include "irkit/coding/hutucker.hpp"
#include "irkit/coding/varbyte.hpp"

namespace {

using namespace irk::coding;

TEST(VarByte, encode)
{
    irk::coding::varbyte_codec<int> vb;
    std::vector<char> actual = encode({1, 255}, vb);
    std::vector<char> expected = {
        (char)0b10000001u, (char)0b01111111, (char)0b10000001};
    EXPECT_THAT(actual, ::testing::ElementsAreArray(expected));
}

TEST(VarByte, encode_fn)
{
    irk::coding::varbyte_codec<int> vb;
    std::vector<std::pair<int, char>> input = {
        std::make_pair(1, 'a'), std::make_pair(255, 'b')};
    std::vector<char> actual =
        encode_fn(input, [](const auto& p) { return p.first; }, vb);
    std::vector<char> expected = {
        (char)0b10000001u, (char)0b01111111, (char)0b10000001};
    EXPECT_THAT(actual, ::testing::ElementsAreArray(expected));
}

TEST(VarByte, decode_range)
{
    irk::coding::varbyte_codec<int> vb;
    std::vector<int> actual =
        decode({(char)0b10000001u, (char)0b01111111, (char)0b10000001}, vb);
    std::vector<int> expected = {1, 255};
    EXPECT_THAT(actual, ::testing::ElementsAreArray(expected));
}

TEST(VarByte, encode_decode)
{
    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution(0, 1000000);
    auto roll = std::bind(distribution, generator);

    irk::coding::varbyte_codec<int> vb;

    // given
    std::vector<int> initial(100);
    std::generate(initial.begin(), initial.end(), roll);

    // when
    std::vector<char> encoded = encode(initial, vb);
    std::vector<int> decoded = decode(encoded, vb);

    // then
    EXPECT_THAT(decoded, ::testing::ElementsAreArray(initial));
}

using node = irk::coding::huffman::node<char>;
using node_ptr = std::shared_ptr<node>;

TEST(Huffman, terminal)
{
    auto a = irk::coding::huffman::make_terminal('a', 2);
    EXPECT_EQ(a->frequency, 2);
    EXPECT_EQ(a->symbol.value(), 'a');
    EXPECT_EQ(a->left, nullptr);
    EXPECT_EQ(a->right, nullptr);
}

TEST(Huffman, join_nodes)
{
    auto a = irk::coding::huffman::make_terminal('a', 2);
    auto b = irk::coding::huffman::make_terminal('b', 4);
    auto ab = irk::coding::huffman::join_nodes(a, b);
    EXPECT_EQ(ab->frequency, 6);
    EXPECT_EQ(ab->symbol, std::nullopt);
    EXPECT_EQ(ab->left->symbol.value(), 'a');
    EXPECT_EQ(ab->right->symbol.value(), 'b');
}

TEST(Huffman, symbol_frequencies)
{
    std::istringstream stream("abcabxdddd");
    auto freqs = irk::coding::huffman::symbol_frequencies(stream);
    EXPECT_EQ(freqs[0], 0);
    EXPECT_EQ(freqs[freqs.size() - 1], 0);
    EXPECT_EQ(freqs['a'], 2);
    EXPECT_EQ(freqs['b'], 2);
    EXPECT_EQ(freqs['c'], 1);
    EXPECT_EQ(freqs['x'], 1);
    EXPECT_EQ(freqs['d'], 4);
}

TEST(Huffman, init_nodes)
{
    std::istringstream stream("abcabxdddd");
    auto freqs = irk::coding::huffman::symbol_frequencies(stream);
    auto nodes = irk::coding::huffman::init_nodes(freqs);
    EXPECT_EQ(nodes.size(), 5);
    auto it = nodes.begin();
    EXPECT_EQ((*it)->symbol.value(), 'a');
    EXPECT_EQ((*it)->frequency, 2);
    EXPECT_EQ((*it)->left, nullptr);
    EXPECT_EQ((*it)->right, nullptr);
    ++it;
    EXPECT_EQ((*it)->symbol.value(), 'b');
    EXPECT_EQ((*it)->frequency, 2);
    EXPECT_EQ((*it)->left, nullptr);
    EXPECT_EQ((*it)->right, nullptr);
    ++it;
    EXPECT_EQ((*it)->symbol.value(), 'c');
    EXPECT_EQ((*it)->frequency, 1);
    EXPECT_EQ((*it)->left, nullptr);
    EXPECT_EQ((*it)->right, nullptr);
    ++it;
    EXPECT_EQ((*it)->symbol.value(), 'd');
    EXPECT_EQ((*it)->frequency, 4);
    EXPECT_EQ((*it)->left, nullptr);
    EXPECT_EQ((*it)->right, nullptr);
    ++it;
    EXPECT_EQ((*it)->symbol.value(), 'x');
    EXPECT_EQ((*it)->frequency, 1);
    EXPECT_EQ((*it)->left, nullptr);
    EXPECT_EQ((*it)->right, nullptr);
    ++it;
    EXPECT_EQ(it, nodes.end());
}

class HuTucker : public ::testing::Test {
protected:
    std::list<node_ptr> nodes = {huffman::make_terminal('a', 4),
        huffman::make_terminal('b', 3),
        huffman::make_terminal('c', 3),
        huffman::make_terminal('d', 5),
        huffman::make_terminal('e', 19)};
};

TEST_F(HuTucker, join_selected_outer)
{
    std::list<node_ptr> n(nodes);
    auto selected = std::pair<node_ptr, node_ptr>(n.front(), n.back());
    hutucker::join_selected(n, selected);
    std::list<node_ptr> expected = {std::make_shared<node>(node{23,
                                        std::nullopt,
                                        huffman::make_terminal('a', 4),
                                        huffman::make_terminal('e', 19)}),
        huffman::make_terminal('b', 3),
        huffman::make_terminal('c', 3),
        huffman::make_terminal('d', 5)};
    EXPECT_THAT(n, ::testing::ElementsAreArray(expected));
}

TEST_F(HuTucker, join_selected_left_outer)
{
    std::list<node_ptr> n(nodes);
    auto selected = std::pair<node_ptr, node_ptr>(
        n.front(), huffman::make_terminal('d', 5));
    hutucker::join_selected(n, selected);
    std::list<node_ptr> expected = {std::make_shared<node>(node{9,
                                        std::nullopt,
                                        huffman::make_terminal('a', 4),
                                        huffman::make_terminal('d', 5)}),
        huffman::make_terminal('b', 3),
        huffman::make_terminal('c', 3),
        huffman::make_terminal('e', 19)};
    EXPECT_THAT(n, ::testing::ElementsAreArray(expected));
}

TEST_F(HuTucker, join_selected_right_outer)
{
    std::list<node_ptr> n(nodes);
    auto selected =
        std::pair<node_ptr, node_ptr>(huffman::make_terminal('b', 3), n.back());
    hutucker::join_selected(n, selected);
    std::list<node_ptr> expected = {huffman::make_terminal('a', 4),
        std::make_shared<node>(node{22,
            std::nullopt,
            huffman::make_terminal('b', 3),
            huffman::make_terminal('e', 19)}),
        huffman::make_terminal('c', 3),
        huffman::make_terminal('d', 5)};
    EXPECT_THAT(n, ::testing::ElementsAreArray(expected));
}

TEST_F(HuTucker, join_selected_inner_adjacent)
{
    std::list<node_ptr> n(nodes);
    auto selected = std::pair<node_ptr, node_ptr>(
        huffman::make_terminal('b', 3), huffman::make_terminal('c', 3));
    hutucker::join_selected(n, selected);
    std::list<node_ptr> expected = {huffman::make_terminal('a', 4),
        std::make_shared<node>(node{6,
            std::nullopt,
            huffman::make_terminal('b', 3),
            huffman::make_terminal('c', 3)}),
        huffman::make_terminal('d', 5),
        huffman::make_terminal('e', 19)};
    EXPECT_THAT(n, ::testing::ElementsAreArray(expected));
}

TEST_F(HuTucker, join_next_valid)
{
    std::list<node_ptr> n(nodes);
    hutucker::join_next_valid(n);
    std::list<node_ptr> expected = {huffman::make_terminal('a', 4),
        std::make_shared<node>(node{6,
            std::nullopt,
            huffman::make_terminal('b', 3),
            huffman::make_terminal('c', 3)}),
        huffman::make_terminal('d', 5),
        huffman::make_terminal('e', 19)};
    EXPECT_THAT(n, ::testing::ElementsAreArray(expected));
}

TEST_F(HuTucker, build_tree)
{
    auto t = huffman::make_terminal<char>;
    auto join = huffman::join_nodes<char>;
    std::list<node_ptr> n(nodes);
    auto tree = hutucker::build_tree(n);
    node_ptr expected =
        join(join(join(t('a', 4), t('d', 5)), join(t('b', 3), t('c', 3))),
            t('e', 19));
    EXPECT_EQ(tree, expected);
}

TEST_F(HuTucker, tag_leaves)
{
    auto t = huffman::make_terminal<char>;
    std::list<node_ptr> n(nodes);
    auto tree = hutucker::build_tree(n);
    auto leaves = hutucker::tag_leaves(tree);
    std::list<hutucker::level_node<char>> expected = {{3, t('a', 4)},
        {3, t('b', 3)},
        {3, t('c', 3)},
        {3, t('d', 5)},
        {1, t('e', 19)}};
    auto eit = expected.begin();
    for (auto lit = leaves.begin();
         lit != leaves.end();
         ++lit, ++eit) {
        EXPECT_EQ(lit->level, eit->level);
        EXPECT_EQ(lit->node, eit->node);
    }
}

TEST_F(HuTucker, reconstruct)
{
    auto t = huffman::make_terminal<char>;
    auto join = huffman::join_nodes_with_symbol<char>;
    std::list<node_ptr> n(nodes);
    auto tree = hutucker::build_tree(n);
    auto leaves = hutucker::tag_leaves(tree);
    auto reconstructed = hutucker::reconstruct(leaves);
    node_ptr expected = join(join(join(t('a', 4), t('b', 3), 'a'),
                                join(t('c', 3), t('d', 5), 'c'),
                                'b'),
        t('e', 19),
        'd');
    EXPECT_EQ(reconstructed, expected);
}

TEST_F(HuTucker, to_compact)
{
    using alphabetical_bst =
        irk::alphabetical_bst<char, uint16_t, std::vector<char>>;
    using node = typename alphabetical_bst::node;

    //auto t = huffman::make_terminal<char>;
    //auto join = huffman::join_nodes_bst<char>;
    std::list<node_ptr> n(nodes);
    auto tree = hutucker::build_tree(n);
    auto leaves = hutucker::tag_leaves(tree);
    auto reconstructed = hutucker::reconstruct(leaves);
    auto compact = hutucker::compact(reconstructed);

    const uint16_t s = 256;
    std::vector<node> expected_nodes = {
        {'d', s + 5, 'e'},
        {'b', s + 10, s + 15},
        {'a', 'a', 'b'},
        {'c', 'c', 'd'},
    };
    std::vector<char> expected_bytes;
    for (auto& node : expected_nodes) {
        expected_bytes.insert(expected_bytes.end(), node.bytes, node.bytes + 5);
    }
    EXPECT_THAT(compact.mem_, ::testing::ElementsAreArray(expected_bytes));
}

TEST_F(HuTucker, with_compact)
{
    std::list<node_ptr> n(nodes);
    auto tree = hutucker::build_tree(n);
    auto leaves = hutucker::tag_leaves(tree);
    auto reconstructed = hutucker::reconstruct(leaves);
    auto compact = hutucker::compact(reconstructed);
    irk::coding::hutucker_codec<char> codec(compact);

    std::string content("abcdaaabbbe");

    // encode
    std::istringstream encode_source(content);
    std::ostringstream encode_string_sink;
    irk::output_bit_stream encode_sink(encode_string_sink);
    codec.encode(encode_source, encode_sink);
    encode_sink.flush();

    // decode
    std::istringstream decode_string_source(encode_string_sink.str());
    irk::input_bit_stream decode_source(decode_string_source);
    std::ostringstream decode_sink;
    codec.decode(decode_source, decode_sink, content.size());

    EXPECT_THAT(decode_sink.str(), ::testing::ElementsAreArray(content));
}

TEST_F(HuTucker, with_frequencies)
{
    std::list<node_ptr> nodes = {huffman::make_terminal('a', 4),
        huffman::make_terminal('b', 3),
        huffman::make_terminal('c', 3),
        huffman::make_terminal('d', 5),
        huffman::make_terminal('e', 19)};
    std::vector<std::size_t> frequencies(128, 0);
    frequencies['a'] = 4;
    frequencies['b'] = 3;
    frequencies['c'] = 3;
    frequencies['d'] = 5;
    frequencies['e'] = 19;
    irk::coding::hutucker_codec<char> codec(frequencies);

    std::string content("abcdaaabbbe");

    // encode
    std::istringstream encode_source(content);
    std::ostringstream encode_string_sink;
    irk::output_bit_stream encode_sink(encode_string_sink);
    codec.encode(encode_source, encode_sink);
    encode_sink.flush();

    // decode
    std::istringstream decode_string_source(encode_string_sink.str());
    irk::input_bit_stream decode_source(decode_string_source);
    std::ostringstream decode_sink;
    codec.decode(decode_source, decode_sink, content.size());

    EXPECT_THAT(decode_sink.str(), ::testing::ElementsAreArray(content));
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}


