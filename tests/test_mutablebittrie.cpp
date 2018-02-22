#include <vector>
#include <memory>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#define private public
#define protected public
#include "irkit/mutablebittrie.hpp"

namespace {

using namespace irk;

TEST(mutable_bit_trie, empty)
{
    irk::mutable_bit_trie<> mbt;
    ASSERT_EQ(mbt.empty(), true);
}

class mutable_bit_trie_uncompressed : public ::testing::Test {
protected:
    irk::mutable_bit_trie<> make_mbt() {
        using node_ptr = irk::mutable_bit_trie<>::node_ptr;
        auto make = [](node_ptr l,
                        node_ptr r,
                        std::optional<bool> sent = std::nullopt) {
            return std::make_shared<irk::mutable_bit_trie<>::node>(l, r, sent);
        };
        node_ptr root = make(
                make(nullptr, nullptr, true),         // 0
                make(                                 // 1-
                    nullptr,
                    make(                             // 11-
                        make(nullptr, nullptr, true), // 110
                        make(nullptr, nullptr, true)  // 111
                    )
                )
            );
        return irk::mutable_bit_trie<>(root);
    }
};

class mutable_bit_trie_compressed : public ::testing::Test {
protected:
    irk::mutable_bit_trie<> make_mbt() {
        using node_ptr = irk::mutable_bit_trie<>::node_ptr;
        auto make = [](node_ptr l,
                        node_ptr r,
                        std::optional<bool> sent = std::nullopt,
                        bitword b = bitword()) {
            return std::make_shared<irk::mutable_bit_trie<>::node>(
                b, l, r, sent);
        };
        node_ptr root = make(
                make(nullptr, nullptr, true),         // 0
                make(                                 // 1-
                    nullptr,
                    make(                             // 10011-
                        make(nullptr, nullptr, true), // 100110
                        make(nullptr, nullptr, true)  // 100111
                    ),
                    std::nullopt,
                    bitword(3, 100)
                )
            );
        return irk::mutable_bit_trie(root);
    }
};

class mutable_bit_trie_mapped : public ::testing::Test {
protected:
    irk::mutable_bit_trie<int> make_mbt() {
        using node_ptr = irk::mutable_bit_trie<int>::node_ptr;
        auto make = [](node_ptr l,
                        node_ptr r,
                        std::optional<int> value = std::nullopt,
                        bitword b = bitword()) {
            return std::make_shared<irk::mutable_bit_trie<int>::node>(
                b, l, r, value);
        };
        auto val = [](int n) { return std::make_optional<int>(n); };
        node_ptr root = make(
                make(nullptr,
                    make(nullptr, nullptr, val(0))      // 01
                    ),
                make(                                   // 1-
                    nullptr,
                    make(                               // 10011-
                        make(nullptr, nullptr, val(1)), // 100110
                        make(nullptr, nullptr, val(2))  // 100111
                    ),
                    std::nullopt,
                    bitword(3, 100)
                )
            );
        return irk::mutable_bit_trie<int>(root);
    }
};

TEST_F(mutable_bit_trie_uncompressed, empty_bitset)
{
    auto mbt = make_mbt();
    EXPECT_EQ(mbt.empty(), false);
    auto[pos, n] = mbt.find(bitword());
    EXPECT_EQ(pos, 0);
    EXPECT_EQ(n, mbt.root_);
}

TEST_F(mutable_bit_trie_uncompressed, insert)
{
    auto mbt = irk::mutable_bit_trie<int>();
    EXPECT_EQ(mbt.empty(), true);
    EXPECT_EQ(mbt.insert(bitword(), 100), false);

    EXPECT_EQ(
        mbt.contains(bitword(1, 0b0)), false);
    EXPECT_EQ(mbt.insert(bitword(1, 0b0), 0), true);
    EXPECT_EQ(mbt.contains(bitword(1, 0b0)), true);
    EXPECT_EQ(mbt.value(bitword(1, 0b0)).value(), 0);

    EXPECT_EQ(mbt.contains(bitword(3, 0b011)), false);
    EXPECT_EQ(mbt.insert(bitword(3, 0b011), 1), true);
    EXPECT_EQ(mbt.contains(bitword(3, 0b011)), true);
    EXPECT_EQ(mbt.value(bitword(3, 0b011)).value(), 1);

    EXPECT_EQ(mbt.contains(bitword(3, 0b111)), false);
    EXPECT_EQ(mbt.insert(bitword(3, 0b111), 2), true);
    EXPECT_EQ(mbt.contains(bitword(3, 0b111)), true);
    EXPECT_EQ(mbt.value(bitword(3, 0b111)).value(), 2);

    EXPECT_EQ(mbt.contains(bitword(2, 0b11)), false);
    EXPECT_EQ(mbt.insert(bitword(2, 0b11), 3), true);
    EXPECT_EQ(mbt.contains(bitword(2, 0b11)), true);
    EXPECT_EQ(mbt.value(bitword(2, 0b11)).value(), 3);

    EXPECT_EQ(mbt.insert(bitword(1, 0b111), 10), false);
    EXPECT_EQ(mbt.insert(bitword(1, 0b011), 10), false);
    EXPECT_EQ(mbt.insert(bitword(1, 0b0), 10), false);
}

TEST_F(mutable_bit_trie_uncompressed, insert_reverse)
{
    auto mbt = irk::mutable_bit_trie<>();
    EXPECT_EQ(mbt.empty(), true);
    EXPECT_EQ(mbt.insert(bitword(), 10), false);

    EXPECT_EQ(mbt.contains(bitword(2, 0b11)), false);
    EXPECT_EQ(mbt.insert(bitword(2, 0b11), 0), true);
    EXPECT_EQ(mbt.contains(bitword(2, 0b11)), true);

    EXPECT_EQ(mbt.contains(bitword(3, 0b111)), false);
    EXPECT_EQ(mbt.insert(bitword(3, 0b111), 1), true);
    EXPECT_EQ(mbt.contains(bitword(3, 0b111)), true);

    EXPECT_EQ(mbt.contains(bitword(3, 0b011)), false);
    EXPECT_EQ(mbt.insert(bitword(3, 0b011), 2), true);
    EXPECT_EQ(mbt.contains(bitword(3, 0b011)), true);

    EXPECT_EQ(mbt.contains(bitword(1, 0b0)), false);
    EXPECT_EQ(mbt.insert(bitword(1, 0b0), 3), true);
    EXPECT_EQ(mbt.contains(bitword(1, 0b0)), true);

    EXPECT_EQ(mbt.insert(bitword(1, 0b111), 10), false);
    EXPECT_EQ(mbt.insert(bitword(1, 0b011), 10), false);
    EXPECT_EQ(mbt.insert(bitword(1, 0b0), 10), false);
}

TEST_F(mutable_bit_trie_uncompressed, existing_node)
{
    auto mbt = make_mbt();
    EXPECT_EQ(mbt.empty(), false);
    {
        auto[pos, n] = mbt.find(bitword(1, 0b0));
        EXPECT_EQ(pos, 1);
        EXPECT_EQ(n, mbt.root_->left);
    }
    {
        auto[pos, n] = mbt.find(bitword(1, 0b1));
        EXPECT_EQ(pos, 1);
        EXPECT_EQ(n, mbt.root_->right);
    }
    {
        auto[pos, n] = mbt.find(bitword(2, 0b11));
        EXPECT_EQ(pos, 2);
        EXPECT_EQ(n, mbt.root_->right->right);
    }
    {
        auto[pos, n] = mbt.find(bitword(3, 0b011));
        EXPECT_EQ(pos, 3);
        EXPECT_EQ(n, mbt.root_->right->right->left);
    }
    {
        auto[pos, n] = mbt.find(bitword(3, 0b111));
        EXPECT_EQ(pos, 3);
        EXPECT_EQ(n, mbt.root_->right->right->right);
    }
}

TEST_F(mutable_bit_trie_uncompressed, nonexisting_node)
{
    auto mbt = make_mbt();
    EXPECT_EQ(mbt.empty(), false);
    {
        auto[pos, n] = mbt.find(bitword(1, 0b10));
        EXPECT_EQ(pos, 1);
        EXPECT_EQ(n, mbt.root_->left);
    }
    {
        auto[pos, n] = mbt.find(bitword(1, 0b01));
        EXPECT_EQ(pos, 1);
        EXPECT_EQ(n, mbt.root_->right);
    }
    {
        auto[pos, n] = mbt.find(bitword(3, 0b0011));
        EXPECT_EQ(pos, 3);
        EXPECT_EQ(n, mbt.root_->right->right->left);
    }
}

TEST_F(mutable_bit_trie_uncompressed, contains)
{
    auto mbt = make_mbt();
    EXPECT_EQ(mbt.contains(bitword(1, 0b0)), true);
    EXPECT_EQ(mbt.contains(bitword(1, 0b1)), false);
    EXPECT_EQ(mbt.contains(bitword(2, 0b11)), false);
    EXPECT_EQ(mbt.contains(bitword(3, 0b011)), true);
    EXPECT_EQ(mbt.contains(bitword(3, 0b111)), true);
    EXPECT_EQ(mbt.contains(bitword(3, 0b110)), false);
}

TEST_F(mutable_bit_trie_compressed, empty_bitset)
{
    auto mbt = make_mbt();
    EXPECT_EQ(mbt.empty(), false);
    auto [pos, n] = mbt.find(bitword());
    EXPECT_EQ(pos, 0);
    EXPECT_EQ(n, mbt.root_);
}

TEST_F(mutable_bit_trie_compressed, existing_node)
{
    auto mbt = make_mbt();
    EXPECT_EQ(mbt.empty(), false);
    {
        auto[pos, n] = mbt.find(bitword(1, 0b0));
        EXPECT_EQ(pos, 1);
        EXPECT_EQ(n, mbt.root_->left);
    }
    {
        auto[pos, n] = mbt.find(bitword(1, 0b1));
        EXPECT_EQ(pos, 1);
        EXPECT_EQ(n, mbt.root_->right);
    }
    {
        auto[pos, n] = mbt.find(bitword(6, 0b011001));
        EXPECT_EQ(pos, 6);
        EXPECT_EQ(n, mbt.root_->right->right->left);
    }
    {
        auto[pos, n] = mbt.find(bitword(5, 0b11001));
        EXPECT_EQ(pos, 5);
        EXPECT_EQ(n, mbt.root_->right->right);
    }
    {
        auto[pos, n] = mbt.find(bitword(6, 0b111001));
        EXPECT_EQ(pos, 6);
        EXPECT_EQ(n, mbt.root_->right->right->right);
    }
}

TEST_F(mutable_bit_trie_compressed, nonexisting_node)
{
    auto mbt = make_mbt();
    EXPECT_EQ(mbt.empty(), false);
    {
        auto[pos, n] = mbt.find(bitword(1, 0b10));
        EXPECT_EQ(pos, 1);
        EXPECT_EQ(n, mbt.root_->left);
    }
    {
        auto[pos, n] = mbt.find(bitword(1, 0b01));
        EXPECT_EQ(pos, 1);
        EXPECT_EQ(n, mbt.root_->right);
    }
    {
        auto[pos, n] = mbt.find(bitword(3, 0b0011));
        EXPECT_EQ(pos, 1);
        EXPECT_EQ(n, mbt.root_->right);
    }
    {
        auto[pos, n] = mbt.find(bitword(7, 0b0111001));
        EXPECT_EQ(pos, 6);
        EXPECT_EQ(n, mbt.root_->right->right->right);
    }
}

TEST_F(mutable_bit_trie_compressed, contains)
{
    auto mbt = make_mbt();
    EXPECT_EQ(mbt.contains(bitword(1, 0b0)), true);
    EXPECT_EQ(mbt.contains(bitword(1, 0b1)), false);
    EXPECT_EQ(mbt.contains(bitword(2, 0b11)), false);
    EXPECT_EQ(mbt.contains(bitword(6, 0b011001)), true);
    EXPECT_EQ(mbt.contains(bitword(6, 0b111001)), true);
    EXPECT_EQ(mbt.contains(bitword(3, 0b110)), false);
}

TEST_F(mutable_bit_trie_mapped, contains)
{
    auto mbt = make_mbt();
    EXPECT_EQ(mbt.contains(bitword(2, 0b10)), true);
    EXPECT_EQ(mbt.contains(bitword(1, 0b1)), false);
    EXPECT_EQ(mbt.contains(bitword(2, 0b11)), false);
    EXPECT_EQ(mbt.contains(bitword(6, 0b011001)), true);
    EXPECT_EQ(mbt.contains(bitword(6, 0b111001)), true);
    EXPECT_EQ(mbt.contains(bitword(3, 0b110)), false);
}

TEST_F(mutable_bit_trie_mapped, value)
{
    auto mbt = make_mbt();
    EXPECT_EQ(mbt.value(bitword(2, 0b10)).value(), 0);
    EXPECT_EQ(mbt.value(bitword(1, 0b1)), std::nullopt);
    EXPECT_EQ(mbt.value(bitword(2, 0b11)), std::nullopt);
    EXPECT_EQ(mbt.value(bitword(6, 0b011001)).value(), 1);
    EXPECT_EQ(mbt.value(bitword(6, 0b111001)).value(), 2);
    EXPECT_EQ(mbt.value(bitword(3, 0b110)), std::nullopt);
}

TEST_F(mutable_bit_trie_mapped, first_lower)
{
    auto mbt = make_mbt();
    {
        auto[exact, lte] = mbt.find_or_first_lower(bitword(2, 0b10));
        auto[pos, found] = mbt.find(bitword(2, 0b10));
        EXPECT_EQ(exact, true);
        EXPECT_EQ(pos, 2);
        EXPECT_EQ(lte, found);
    }
    {
        auto[exact, lte] = mbt.find_or_first_lower(bitword(1, 0b1));
        auto[pos, found] = mbt.find(bitword(2, 0b10));
        EXPECT_EQ(exact, false);
        EXPECT_EQ(pos, 2);
        EXPECT_EQ(lte, found);
    }
    {
        auto[exact, lte] = mbt.find_or_first_lower(bitword(1, 0b11));
        auto[pos, found] = mbt.find(bitword(2, 0b10));
        EXPECT_EQ(exact, false);
        EXPECT_EQ(pos, 2);
        EXPECT_EQ(lte, found);
    }
    {
        auto[exact, lte] = mbt.find_or_first_lower(bitword(6, 0b011001));
        auto[pos, found] = mbt.find(bitword(6, 0b011001));
        EXPECT_EQ(exact, true);
        EXPECT_EQ(pos, 6);
        EXPECT_EQ(lte, found);
    }
    {
        auto[exact, lte] = mbt.find_or_first_lower(bitword(6, 0b111001));
        auto[pos, found] = mbt.find(bitword(6, 0b111001));
        EXPECT_EQ(exact, true);
        EXPECT_EQ(pos, 6);
        EXPECT_EQ(lte, found);
    }
    {
        auto[exact, lte] = mbt.find_or_first_lower(bitword(6, 0b110));
        auto[pos, found] = mbt.find(bitword(2, 0b10));
        EXPECT_EQ(exact, false);
        EXPECT_EQ(pos, 2);
        EXPECT_EQ(lte, found);
    }
    {
        auto[exact, lte] = mbt.find_or_first_lower(bitword(1, 0b0));
        EXPECT_EQ(exact, false);
        EXPECT_EQ(lte, nullptr);
    }
    //EXPECT_EQ(mbt.value(bitword(3, 0b110)), std::nullopt);
}

TEST_F(mutable_bit_trie_mapped, items)
{
    auto mbt = make_mbt();
    std::vector<std::pair<bitword, int>> mapping;
    mbt.items(mbt.root_, bitword(), mapping);
    std::sort(
        mapping.begin(), mapping.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.second < rhs.second;
        });
    EXPECT_EQ(mapping.size(), 3);
    EXPECT_EQ(mapping[0], std::make_pair(bitword(2, 0b10), 0));
    EXPECT_EQ(mapping[1], std::make_pair(bitword(6, 0b011001), 1));
    EXPECT_EQ(mapping[2], std::make_pair(bitword(6, 0b111001), 2));
}

TEST_F(mutable_bit_trie_mapped, dump)
{
    auto mbt = make_mbt();
    std::ostringstream out;
    mbt.dump(out);
    std::string outstr = out.str();
    std::vector<char> bytes(outstr.begin(), outstr.end());
    std::vector<char> expected = {3, 0, 0, 0, 0, 0, 0, 0,
        (char)0b10000000, (char)0b10000010, 0b10, // 0
        (char)0b10000001, (char)0b10000110, 0b011001, // 1
        (char)0b10000010, (char)0b10000110, 0b111001  // 2
    };
    EXPECT_THAT(bytes, ::testing::ElementsAreArray(expected));

    std::istringstream in(outstr);
    auto loaded_mbt = load_mutable_bit_trie<int>(in);
    std::vector<std::pair<bitword, int>> mapping;
    mbt.items(loaded_mbt.root_, bitword(), mapping);
    std::sort(
        mapping.begin(), mapping.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.second < rhs.second;
        });
    EXPECT_EQ(mapping.size(), 3);
    EXPECT_EQ(mapping[0], std::make_pair(bitword(2, 0b10), 0));
    EXPECT_EQ(mapping[1], std::make_pair(bitword(6, 0b011001), 1));
    EXPECT_EQ(mapping[2], std::make_pair(bitword(6, 0b111001), 2));
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

