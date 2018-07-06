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

//! \file alphabetical_bst.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <boost/concept_check.hpp>
#include <boost/dynamic_bitset.hpp>
#include <cstdint>
#include <vector>
#include "irkit/bitstream.hpp"

namespace irk {

//! Read-only array-based representation of an Alphabetic Binary Search Tree
/*!
 * @tparam Symbol           symbol type, `char` by default
 * @tparam Ptr              the type of the children pointers; determines the
 *                          maximum size of the tree;
 * @tparam MemoryContainer  memory container type, std::vector<char> by default;
 *                          must implement data() and size(); data() should
 *                          implement operator[]
 */
template<class Symbol = char,
    class Ptr = std::uint16_t,
    class MemoryContainer = std::vector<char>>
class alphabetical_bst {
    BOOST_CONCEPT_ASSERT((boost::RandomAccessContainer<MemoryContainer>));
    static_assert(sizeof(Symbol) < sizeof(Ptr));

public:
    using symbol_type = Symbol;
    using pointer_type = Ptr;
    using container_type = MemoryContainer;
    using self_type =
        alphabetical_bst<symbol_type, pointer_type, container_type>;

private:
    container_type mem_;

public:
    static constexpr std::size_t symbol_bound = 1 << (sizeof(symbol_type) * 8);
    static constexpr std::size_t node_size =
        sizeof(symbol_type) + sizeof(pointer_type) * 2;
    static constexpr std::size_t symbol_offset = 0;
    static constexpr std::size_t left_offset = sizeof(symbol_type);
    static constexpr std::size_t right_offset =
        sizeof(pointer_type) + sizeof(symbol_type);

    alphabetical_bst() = default;
    alphabetical_bst(const self_type&) = default;
    alphabetical_bst(self_type&&) = default;
    alphabetical_bst(container_type mem) : mem_(std::move(mem)) {}
    self_type& operator=(const self_type&) = default;
    self_type& operator=(self_type&&) = default;

    struct node_ptr {
        const char* bytes;
        const symbol_type& symbol() const {
            return *reinterpret_cast<const symbol_type*>(bytes + symbol_offset);
        };
        const pointer_type& left() const {
            return *reinterpret_cast<const pointer_type*>(bytes + left_offset);
        };
        const pointer_type& right() const {
            return *reinterpret_cast<const pointer_type*>(bytes + right_offset);
        };
    };

    struct node {
        char bytes[node_size];
        node(symbol_type symbol)
        {
            memcpy(bytes + symbol_offset, &symbol, sizeof(symbol_type));
        }
        node(symbol_type symbol, pointer_type left, pointer_type right)
        {
            memcpy(bytes + symbol_offset, &symbol, sizeof(symbol_type));
            memcpy(bytes + left_offset, &left, sizeof(pointer_type));
            memcpy(bytes + right_offset, &right, sizeof(pointer_type));
        }
        const node_ptr ptr() const { return node_ptr(bytes); }
        const symbol_type& symbol() const {
            return ptr().symbol();
        };
        const pointer_type& left() const {
            return ptr().left();
        };
        const pointer_type& right() const {
            return ptr().right();
        };
    };

    node_ptr root() const { return node_at(0); }
    node_ptr node_at(pointer_type ptr) const
    {
        return node_ptr{mem_.data() + ptr};
    }

    const container_type& memory_container() const { return mem_; }

    void encode(symbol_type symbol, output_bit_stream& sink) const
    {
        node_ptr current_node = root();
        while (true) {
            bool goright = symbol > current_node.symbol();
            pointer_type next =
                goright ? current_node.right() : current_node.left();
            sink.write(goright);
            if (next < symbol_bound) {
                break;
            }
            current_node = node_at(next - symbol_bound);
        }
    }

    void
    encode(symbol_type symbol, boost::dynamic_bitset<unsigned char>& sink) const
    {
        node_ptr current_node = root();
        while (true) {
            bool goright = symbol > current_node.symbol();
            pointer_type next =
                goright ? current_node.right() : current_node.left();
            sink.push_back(goright);
            if (next < symbol_bound) {
                break;
            }
            current_node = node_at(next - symbol_bound);
        }
    }

    boost::dynamic_bitset<unsigned char> encode(symbol_type symbol) const
    {
        boost::dynamic_bitset<unsigned char> code;
        node_ptr node = root();
        while (true) {
            bool goright = symbol > node.symbol();
            pointer_type next = goright ? node.right() : node.left();
            code.push_back(goright);
            if (next < symbol_bound) {
                return code;
            }
            node = node_at(next - symbol_bound);
        }
    }

    symbol_type decode(const boost::dynamic_bitset<unsigned char>& bits) const
    {
        pointer_type next(symbol_bound);
        node_ptr node;
        for (std::size_t idx = 0; idx < bits.size(); ++idx) {
            bool goright = bits[idx];
            node = node_at(next - symbol_bound);
            next = goright ? node.right() : node.left();
        }
        return *reinterpret_cast<const symbol_type*>(&next);
    }

    template<class InputBitStream>
    symbol_type decode(InputBitStream& source) const
    {
        pointer_type next(symbol_bound);
        node_ptr node;
        while (next >= symbol_bound) {
            std::int8_t bit = source.read();
            if (bit == -1) {
                throw std::runtime_error(
                    "bit stream ended before finishing decoding a symbol");
            }
            node = node_at(next - symbol_bound);
            next = bit ? node.right() : node.left();
        }
        return *reinterpret_cast<const symbol_type*>(&next);
    }
};

};  // namespace irk
