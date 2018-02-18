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
#include <cstring>
#include <list>
#include <vector>
#include "irkit/mutablebittrie.hpp"
#include "irkit/utils.hpp"
#include "irkit/coding/varbyte.hpp"

namespace irk {

template<class V1, class V2>
constexpr bool both_or_none()
{
    if constexpr (!std::is_same<V1, void>::value) {
        return !std::is_same<V2, void>::value;
    } else {
        return std::is_same<V2, void>::value;
    }
};

//! In-memory or disk-based, immutable implementation of Bitwise Trie.
template<class Value = void, class PrefixCodec = void>
class immutable_bit_trie {
    static_assert(both_or_none<Value, PrefixCodec>(),
        "either Value and PrefixCodec both should be defined or neither");

private:
    std::vector<char> mem_;
    PrefixCodec codec_;

    //! Internal node representation.
    /*!
     * Header byte format:
     *  1: is compressed?
     *  2: is word sentinel?
     *  - compressed:
     *    3:   has next
     *    4-8: the length in bits of the compressed path
     *  - uncompressed:
     *    3:   has left
     *    4:   has right
     *    5-8: the length in bytes of the right child's pointer
     *
     * Body:
     *  1. if Value != void and is sentinel [2], then the mapped value is
     *     encoded as follows:
     *     - the number of the leading 1 bits in the first byte determines the
     *       number of used bytes for this value (excluding the first byte);
     *       therefore, the longest values that can be encoded are 8-byte long
     *     - then, a zero bit is encoded
     *
     *  2.
     *    - compressed: the path of the size defined by [4-8]
     *    - uncompressed: the right child ptr of the size defined by [5-8]
     *
     * Following:
     *  - compressed: the next node if has next [3] is true; undefined otherwise
     *  - uncompressed: the left child if has left [3] is true; undefined
     *                  otherwise
     */
    class NodePtr {
    private:
        const char* mem_;

    public:
        NodePtr(const char* mem) : mem_(mem) {}

        constexpr std::size_t header_size() const { return 1; }
        bool compressed() const { return ((*mem_) & 0b10000000) > 0; }
        bool sentinel() const { return ((*mem_) & 0b01000000) > 0; }
        bool has_next() const { return ((*mem_) & 0b00100000) > 0; }
        bool has_left() const { return has_next(); }
        bool has_right() const { return ((*mem_) & 0b00010000) > 0; }
        constexpr std::uint32_t value_size() const
        {
            if constexpr (std::is_same<Value, void>::value) {
                return 0;
            } else {
                return sizeof(Value) * sentinel();
            }
        };
        std::uint32_t rptrlen() const { return (*mem_) & 0b00001111; };
        std::size_t compressed_len() const { return (*mem_) & 0b00011111; };

        boost::dynamic_bitset<unsigned char> compressed_bits() const
        {
            std::size_t len = compressed_len();
            std::size_t bytes = (len + 7) / 8;
            auto bits = boost::dynamic_bitset<unsigned char>(
                mem_ + header_size() + value_size(),
                mem_ + header_size() + value_size() + bytes);
            bits.resize(len);
            return bits;
        };

        /*!
         * The left child (if exists) is directly after the right child's
         * pointer.
         */
        NodePtr left() const
        {
            return NodePtr(mem_ + header_size() + rptrlen() + value_size());
        }

        NodePtr right() const
        {
            std::size_t shift = 0;
            if (rptrlen() > 0) {
                std::memcpy(&shift, mem_ + header_size(), rptrlen());
            }
            return NodePtr(mem_ + header_size() + shift);
        }

        NodePtr next() const
        {
            std::size_t bytes = (compressed_len() + 7) / 8;
            return NodePtr(mem_ + header_size() + value_size() + bytes);
        }

        Value value() const
        {
            const char* valptr = mem_ + header_size();
            Value val;
            std::memcpy(&val, valptr, value_size());
            return val;
        }
    };

    //! An actual single node object used for building.
    class Node {
    private:
        std::vector<unsigned char> mem_;

    public:
        Node(bool has_left, std::size_t rightptr, bool sentinel)
        {
            assert(rightptr < 32);
            unsigned char header = 0;
            header |= sentinel << 6;
            header |= has_left << 5;
            header |= (rightptr > 0) << 4;
            std::uint32_t rptrlen = irk::nbytes(rightptr);
            header |= rptrlen;
            mem_.push_back(header);
            std::vector<unsigned char> rightptr_data(rptrlen, 0);
            std::memcpy(rightptr_data.data(), &rightptr, rptrlen);
            mem_.insert(mem_.end(), rightptr_data.begin(), rightptr_data.end());
        }
    };

    //std::pair<std::size_t,
    //    std::vector<std::pair<MutableBitTrie::NodePtr, std::size_t>>>
    //mutable_tree_sizes(MutableBitTrie::NodePtr mbt)
    //{
    //    if (mbt == nullptr) {
    //        return std::make_pair(0,
    //            std::vector<std::pair<MutableBitTrie::NodePtr, std::size_t>>());
    //    }
    //    auto[left_size, left_sizes] = mutable_tree_sizes(mbt->left);
    //    auto[right_size, right_sizes] = mutable_tree_sizes(mbt->right);

    //    std::vector<std::pair<MutableBitTrie::NodePtr, std::size_t>> sizes;

    //    // Header size
    //    std::size_t size = 1;
    //    std::size_t rightptr = left_size;
    //    if (rightptr > 0) {
    //        size += irk::nbytes(rightptr);
    //    }
    //    sizes.push_back(std::make_pair(mbt, size));

    //    // Children
    //    size += left_size + right_size;
    //    sizes.insert(sizes.end(), left_sizes.begin(), left_sizes.end());
    //    sizes.insert(sizes.end(), right_sizes.begin(), right_sizes.end());

    //    return std::make_pair(size, sizes);
    //}

public:
    immutable_bit_trie(const mutable_bit_trie<Value>& mbt)
    {
        // using MNodePtr = MutableBitTrie::NodePtr;
        // auto[size, sizes] = mutable_tree_sizes(mbt.root());
    }
};

};  // namespace irk
