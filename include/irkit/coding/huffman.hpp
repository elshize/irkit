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

//! \file
//! \author     Michal Siedlaczek
//! \copyright  MIT License

#pragma once

#include <iostream>
#include <list>
#include <memory>
#include <optional>
#include <vector>

namespace irk::coding {

//! This namespace contains the internal implementation of structures and
//! algorithms related to Huffman coding and trees.
namespace huffman {

    //! A structure representing a node in a Huffman coding tree.
    template<class Symbol = char>
    struct node {
        std::size_t frequency;
        std::optional<Symbol> symbol;
        std::shared_ptr<node<Symbol>> left;
        std::shared_ptr<node<Symbol>> right;
        std::size_t level = 0;
        node(std::size_t frequency,
            std::optional<Symbol> symbol,
            std::shared_ptr<node<Symbol>> left,
            std::shared_ptr<node<Symbol>> right)
            : frequency(frequency), symbol(symbol), left(left), right(right)
        {}
        bool operator==(const node<Symbol>& rhs) const
        {
            return frequency == rhs.frequency && symbol == rhs.symbol
                && ((left == nullptr && right == nullptr)
                       || (*left == *rhs.left && *left == *rhs.left));
        }
    };

    template<class Symbol>
    bool operator==(const std::shared_ptr<node<Symbol>>& lhs,
        const std::shared_ptr<node<Symbol>>& rhs)
    {
        if (lhs == nullptr) {
            return rhs == nullptr;
        }
        return lhs->frequency == rhs->frequency && lhs->symbol == rhs->symbol
            && lhs->left == rhs->left && lhs->right == rhs->right;
    }

    template<class Symbol>
    std::ostream&
    operator<<(std::ostream& out, const std::shared_ptr<node<Symbol>>& n)
    {
        out << "[" << n->frequency << ":";
        if (n->symbol != std::nullopt) {
            auto v = n->symbol.value();
            if (v >= 0) {
                out << v;
            } else {
                out << (int)v;
            }
        } else {
            out << "null";
        }
        if (n->left != nullptr) {
            out << " " << n->left << n->right;
        }
        return out << "]";
    }

    //! Creates a terminal node.
    template<class Symbol = char>
    std::shared_ptr<node<Symbol>>
    make_terminal(Symbol symbol, std::size_t frequency)
    {
        return std::make_shared<node<Symbol>>(
            frequency, std::make_optional(symbol), nullptr, nullptr);
    }

    //! Joins two nodes (or subtrees).
    template<class Symbol = char>
    std::shared_ptr<node<Symbol>> join_nodes(
        std::shared_ptr<node<Symbol>> left, std::shared_ptr<node<Symbol>> right)
    {
        std::size_t frequency = left->frequency + right->frequency;
        return std::make_shared<node<Symbol>>(
            frequency, std::nullopt, left, right);
    }

    //! Joins two nodes (or subtrees), preserving the given symbol.
    template<class Symbol = char>
    std::shared_ptr<node<Symbol>>
    join_nodes_with_symbol(std::shared_ptr<node<Symbol>> left,
        std::shared_ptr<node<Symbol>> right,
        Symbol symbol)
    {
        std::size_t frequency = left->frequency + right->frequency;
        return std::make_shared<node<Symbol>>(
            frequency, std::make_optional(symbol), left, right);
    }

    //! Joins two nodes (or subtrees), preserving the symbol according to BST.
    template<class Symbol = char>
    std::shared_ptr<node<Symbol>> join_nodes_bst(
        std::shared_ptr<node<Symbol>> left, std::shared_ptr<node<Symbol>> right)
    {
        std::size_t frequency = left->frequency + right->frequency;
        return std::make_shared<node<Symbol>>(
            frequency, left->symbol, left, right);
    }

    //! Returns a vector of frequencies of all symbols.
    template<class Symbol = char>
    std::vector<std::size_t> symbol_frequencies(std::istream& stream)
    {
        std::size_t symbol_size = sizeof(Symbol);
        std::size_t alphabet_size = 1 << (symbol_size * 8);
        std::vector<std::size_t> frequencies(alphabet_size, 0);
        Symbol symbol;
        while (stream.read(reinterpret_cast<char*>(&symbol), symbol_size)) {
            ++frequencies[static_cast<std::size_t>(symbol)];
        }
        return frequencies;
    }

    //! Initialize all external nodes according to the given frequencies.
    template<class Symbol = char>
    std::list<std::shared_ptr<node<Symbol>>>
    init_nodes(const std::vector<std::size_t>& frequencies)
    {
        std::vector<std::shared_ptr<node<Symbol>>> terminals;
        for (std::size_t symbol_number = 0; symbol_number < frequencies.size();
             ++symbol_number) {
            std::size_t frequency = frequencies[symbol_number];
            if (frequency > 0) {
                Symbol symbol = static_cast<Symbol>(symbol_number);
                terminals.push_back(make_terminal(symbol, frequency));
            }
        }
        std::sort(terminals.begin(),
            terminals.end(),
            [](const auto& lhs, const auto& rhs) {
                return lhs->symbol < rhs->symbol;
            });
        std::list<std::shared_ptr<node<Symbol>>> terminal_list(
            terminals.begin(), terminals.end());
        return terminal_list;
    }

};  // namespace huffman

};  // namespace irk::coding
