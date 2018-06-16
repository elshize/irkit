// MIT License
//
// Copyright (c) 2018 Michal Siedlaczek
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

//! \file mutablebittrie.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <boost/dynamic_bitset.hpp>
#include <memory>
#include <optional>
#include <vector>
#include <irkit/types.hpp>
#include <irkit/coding/varbyte.hpp>
#include <irkit/coding.hpp>

namespace irk {

//! In-memory, mutable implementation of Bitwise Trie.
/*!
 * This implementation is not optimized for efficiency (doesn't necessarily mean
 * it isn't for most applications). Instead, it is meant to be used mainly for
 * building tries, or whenever a mutable version is needed.
 *
 * In particular, this implementation is not conservative when it comes to
 * memory usage. Ease of use is a priority here.
 */
template<class Value = bool>
class mutable_bit_trie {
public:
    struct node;
    using node_ptr = std::shared_ptr<node>;
    using value_type = Value;
    using value_opt = std::optional<value_type>;

    struct node {
        bitword bits;
        node_ptr left, right;
        bool sentinel;
        value_opt value;

        node(node_ptr left = nullptr,
            node_ptr right = nullptr,
            value_opt value = std::nullopt)
            : bits({}),
              left(left),
              right(right),
              sentinel(value != std::nullopt),
              value(value)
        {}

        node(bitword bits,
            node_ptr left = nullptr,
            node_ptr right = nullptr,
            value_opt value = std::nullopt)
            : bits(bits),
              left(left),
              right(right),
              sentinel(value != std::nullopt),
              value(value)
        {}

        bool compressed() const { return bits.size() > 0; }
    };

private:
    node_ptr root_;

    void insert_node(
        node_ptr n, const bitword& encoded, std::size_t bitn, Value value)
    {
        bool first_bit = encoded.test(bitn++);
        auto bits = encoded >> bitn;
        bits.resize(encoded.size() - bitn);
        node_ptr new_node = std::make_shared<node>(
            bits, nullptr, nullptr, std::make_optional(value));
        if (first_bit) {
            n->right = new_node;
        } else {
            n->left = new_node;
        }
    }

    void break_node(node_ptr n, bitword&& prefix)
    {
        bool right = n->bits[prefix.size()];
        bitword suffix = n->bits >> (prefix.size() + 1);
        suffix.resize(n->bits.size() - prefix.size() - 1);
        n->bits = prefix;
        node_ptr new_node = std::make_shared<node>(
            suffix, n->left, n->right, n->value);
        if (right) {
            n->right = new_node;
            n->left = nullptr;
        } else {
            n->left = new_node;
            n->right = nullptr;
        }
        n->value = std::nullopt;
        n->sentinel = false;
    }

    bool make_external(node_ptr n, Value value)
    {
        bool exists = n->sentinel;
        n->sentinel = true;
        n->value = std::make_optional(value);
        return !exists;
    }

    std::pair<std::size_t, bitword> common(std::size_t bitpos,
        const bitword& inserted,
        const bitword& internal) const
    {
        bitword common;
        while (common.size() < internal.size() && bitpos < inserted.size()
            && inserted[bitpos] == internal[common.size()]) {
            common.push_back(inserted[bitpos++]);
        }
        return std::make_pair(bitpos, std::move(common));
    }

    node_ptr find_right_most_external_of(node_ptr n) const
    {
        if (n == nullptr) {
            return nullptr;
        }
        if (n->right != nullptr) {
            return find_right_most_external_of(n->right);
        } else if (n->sentinel) {
            return n;
        } else {
            return find_right_most_external_of(n->left);
        }
    }

    struct search_result {
        bool exact;
        std::size_t match_node_beg;
        std::size_t match_prefix;
        node_ptr match;
        node_ptr last_left;
    };

    search_result search(const bitword& encoded, std::size_t begin = 0) const
    {
        node_ptr current_node = root_;
        node_ptr last_left = nullptr;
        std::size_t pos = begin;
        while (pos < encoded.size()) {
            std::size_t node_beg = pos;
            std::size_t bidx = 0;
            for (; bidx < current_node->bits.size() && pos < encoded.size();
                 ++bidx, ++pos) {
                if (encoded[pos] != current_node->bits[bidx]) {
                    return {false, node_beg, pos, current_node, last_left};
                }
            }
            if (pos == encoded.size()) {
                if (bidx == current_node->bits.size()) {
                    return {true, node_beg, pos, current_node, last_left};
                } else {
                    return {false, node_beg, pos, current_node, last_left};
                }
            }
            node_ptr child =
                encoded[pos] ? current_node->right : current_node->left;
            if (encoded[pos]) {
                if (current_node->sentinel) {
                    last_left = current_node;
                } else if (current_node->left != nullptr) {
                    last_left = current_node->left;
                }
            }
            if (child != nullptr) {
                current_node = child;
                ++pos;
            } else {
                return {false, node_beg, pos, current_node, last_left};
            }
        }
        return {current_node->bits.size() == 0,
            encoded.size(),
            encoded.size(),
            current_node,
            last_left};
    }

    void print(std::ostream& out, node_ptr n) const
    {
        if (n == nullptr) {
            out << "#";
            return;
        }
        out << "[";
        if (n->sentinel) {
            out << "*";
        }
        if (n->value != std::nullopt) {
            out << "{" << n->value.value() << "} ";
        }
        out << n->bits;
        print(out, n->left);
        print(out, n->right);
        out << "]";
    }

    void items(node_ptr n,
        bitword current,
        std::vector<std::pair<bitword, value_type>>& mapping) const
    {
        if (n->bits.size() > 0) {
            for (std::size_t idx = 0; idx < n->bits.size(); ++idx) {
                current.push_back(n->bits[idx]);
            }
        }
        if (n->value != std::nullopt) {
            mapping.emplace_back(current, n->value.value());
        }
        if (n->left != nullptr) {
            bitword left_bitword = current;
            left_bitword.push_back(0);
            items(n->left, left_bitword, mapping);
        }
        if (n->right != nullptr) {
            bitword right_bitword = current;
            right_bitword.push_back(1);
            items(n->right, right_bitword, mapping);
        }
    }

public:
    mutable_bit_trie() : root_(std::make_shared<node>()) {}
    mutable_bit_trie(node_ptr root) : root_(root) {}
    mutable_bit_trie(const mutable_bit_trie<value_type>& other)
        : root_(other.root_)
    {}

    node_ptr root() const { return root_; }

    // TODO: disable for other than `void` -- class specialization?
    bool insert(const bitword& encoded) { return insert(encoded, true); }

    bool
    insert(const bitword& encoded, Value value)
    {
        std::size_t bitn = 0;
        if (encoded.size() == 0) {
            return false;
        }
        auto[pos, insertion_node] = find(encoded);
        bitn = pos;
        bool found_as_external = bitn == encoded.size();
        if (found_as_external) {
            return make_external(insertion_node, value);
        } else {
            if (!insertion_node->compressed()) {
                insert_node(insertion_node, encoded, bitn, value);
                return true;
            } else {
                auto[bitpos, common_prefix] =
                    common(bitn, encoded, insertion_node->bits);
                bitn = bitpos;
                if (common_prefix.size() < insertion_node->bits.size()) {
                    break_node(insertion_node, std::move(common_prefix));
                }
                if (bitn < encoded.size()) {
                    insert_node(insertion_node, encoded, bitn, value);
                    return true;
                } else {
                    bool inserted = !insertion_node->sentinel;
                    insertion_node->sentinel = true;
                    insertion_node->value = value;
                    return inserted;
                }
            }
        }
    }

    std::pair<bool, node_ptr> find_or_first_lower(const bitword& encoded) const
    {
        search_result result = search(encoded);
        std::cout << result.match_prefix << std::endl;
        if (result.match_prefix == encoded.size()) {
            if (result.exact && result.match->sentinel) {
                return std::make_pair(true, result.match);
            }
            auto first_lower = find_right_most_external_of(result.last_left);
            return std::make_pair(false, first_lower);
        }
        if (encoded[result.match_prefix] == 0) {
            if (!result.match->compressed()) {
                auto first_lower =
                    find_right_most_external_of(result.match->left);
                return std::make_pair(false, first_lower);
            }
            if (result.match_node_beg + result.match->bits.size()
                > result.match_prefix) {
                auto first_lower =
                    find_right_most_external_of(result.last_left);
                return std::make_pair(false, first_lower);
            }
            return std::make_pair(false, result.match);
        } else {
            if (result.match->sentinel) {
                return std::make_pair(false, result.match);
            }
            if (result.last_left != nullptr) {
                auto first_lower =
                    find_right_most_external_of(result.last_left);
                return std::make_pair(false, first_lower);
            }
            auto first_lower = find_right_most_external_of(result.match);
            return std::make_pair(false, first_lower);
        }
    }

    std::pair<std::size_t, node_ptr> find(const bitword& encoded) const
    {
        search_result result = search(encoded);
        return std::make_pair(result.match_node_beg, result.match);
    }

    bool contains(const bitword& encoded) const
    {
        search_result result = search(encoded);
        return result.match_prefix == encoded.size() && result.exact
            && result.match->sentinel;
    }

    std::optional<Value> value(const bitword& encoded) const
    {
        search_result result = search(encoded);
        return (result.match_prefix == encoded.size() && result.exact
                   && result.match->sentinel)
            ? result.match->value
            : std::nullopt;
    }

    bool empty() const
    {
        return root_->left == nullptr && root_->right == nullptr;
    }

    std::ostream& dump(std::ostream& out) const
    {
        std::vector<std::pair<bitword, value_type>> mapping;
        items(root_, bitword(), mapping);
        std::sort(mapping.begin(),
            mapping.end(),
            [](const auto& lhs, const auto& rhs) {
                return lhs.second < rhs.second;
            });
        std::size_t size = mapping.size();
        out.write(reinterpret_cast<char*>(&size), sizeof(size));
        varbyte_codec<value_type> value_codec;
        varbyte_codec<std::size_t> size_codec;
        for (const auto & [bits, val] : mapping) {
            value_codec.encode(val, out);
            encode_bits(bits, out, size_codec);
        }
        return out;
    }
    template<class V>
    friend std::ostream&
    operator<<(std::ostream& out, const mutable_bit_trie<V>& mbt);
};

template<class Value>
std::ostream& operator<<(std::ostream& out, const mutable_bit_trie<Value>& mbt)
{
    mbt.print(out, mbt.root());
    return out;
}

template<class Value>
mutable_bit_trie<Value> load_mutable_bit_trie(std::istream& in)
{
    mutable_bit_trie<Value> mbt;
    std::vector<std::pair<bitword, Value>> mapping;
    varbyte_codec<Value> value_codec;
    varbyte_codec<std::size_t> size_codec;
    std::size_t size;
    in.read(reinterpret_cast<char*>(&size), sizeof(size));
    for (std::size_t idx = 0; idx < size; ++idx) {
        Value v;
        value_codec.decode(in, v);
        auto bits = decode_bits(in, size_codec);
        mbt.insert(bits, v);
    }
    return mbt;
}

};  // namespace irk
