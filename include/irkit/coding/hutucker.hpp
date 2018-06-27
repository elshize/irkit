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

#include <gsl/gsl_assert>
#include <debug_assert.hpp>

#include <irkit/assert.hpp>
#include <irkit/alphabetical_bst.hpp>
#include <irkit/coding/huffman.hpp>
#include <irkit/types.hpp>

namespace irk {

/*!
 * This namespace contains implementation of the internal mechanics behind the
 * Hu-Tucker codec.
 */
namespace coding::hutucker {

    template<class Symbol = char>
    using node_ptr = std::shared_ptr<huffman::node<Symbol>>;

    //! Stores a Huffman tree node pointer along with its level (height).
    template<class Symbol = char>
    struct level_node {
        std::size_t level;
        node_ptr<Symbol> node;
    };

    //! Joins together a pair of selected nodes in a forest.
    /*!
     * @param forest    a list of tree nodes
     * @param selected  a pair of nodes to join
     *
     * Both `selected` nodes must be in `forest`. Otherwise, the result of this
     * function is undefined.
     */
    template<class Symbol = char>
    void join_selected(std::list<node_ptr<Symbol>>& forest,
        std::pair<typename std::list<node_ptr<Symbol>>::iterator,
            typename std::list<node_ptr<Symbol>>::iterator> selected)
    {
        Expects(forest.size() > 1);
        Expects(selected.first != forest.end());
        Expects(selected.second != forest.end());
        auto joined = join_nodes(*selected.first, *selected.second);
        *selected.first = joined;
        forest.erase(selected.second);
    }

    //! Selects and joins the next valid pair in the forest.
    template<class Symbol = char>
    void join_next_valid(std::list<node_ptr<Symbol>>& forest)
    {
        using node_ptr = node_ptr<Symbol>;
        using iterator_type = typename std::list<node_ptr>::iterator;
        //std::pair<node_ptr, node_ptr> selected =
        //    std::make_pair<node_ptr, node_ptr>(nullptr, nullptr);
        std::pair<iterator_type, iterator_type> selected =
            std::make_pair(forest.end(), forest.end());
        std::size_t selected_freq = std::numeric_limits<std::size_t>::max();
        for (auto it = forest.begin(); it != forest.end(); ++it) {
            auto left = *it;
            auto right_it = std::next(it);
            while (right_it != forest.end()) {
                auto right = *right_it;
                auto freq = left->frequency + right->frequency;
                if (selected_freq > freq) {
                    //selected = std::make_pair(left, right);
                    selected = std::make_pair(it, right_it);
                    selected_freq = freq;
                }
                if (right->symbol != std::nullopt) {
                    break;
                }
                ++right_it;
            }
        }
        join_selected(forest, selected);
    }

    //! Constructs a first-phase Hu-Tucker tree from a list of external nodes.
    /*!
     * @param nodes a list of external nodes sorted lexicographically
     * @returns     a pointer to the root of the constructed tree
     */
    template<class Symbol = char>
    node_ptr<Symbol> build_tree(std::list<node_ptr<Symbol>>& nodes)
    {
        EXPECTS(nodes.size() > 0);
        while (nodes.size() > 1) {
            join_next_valid(nodes);
        }
        ENSURES(nodes.size() == 1);
        ENSURES(nodes.front() != nullptr);
        return nodes.front();
    }

    //! Returns an lexicographically ordered list of leaves tagged by their
    //! height in the tree.
    /*!
     * @param root  the root of the tree
     * @returns     a lexicographically sorted list of terminal nodes
     */
    template<class Symbol = char>
    std::list<level_node<Symbol>> tag_leaves(node_ptr<Symbol> root)
    {
        if (root->symbol.has_value()) {
            return {{0, root}};
        }
        std::vector<level_node<Symbol>> leaves;
        std::list<level_node<Symbol>> stack;
        level_node<Symbol> current = {0, root};
        while (true) {
            if (current.node->symbol != std::nullopt) {
                // Leaf.
                leaves.push_back(current);
                if (stack.empty()) {
                    break;
                }
                current = stack.back();
                stack.pop_back();
            } else {
                stack.push_back({current.level + 1, current.node->right});
                current = {current.level + 1, current.node->left};
            }
        }
        std::sort(
            leaves.begin(), leaves.end(), [](const auto& lhs, const auto& rhs) {
                return lhs.node->symbol < rhs.node->symbol;
            });
        return std::list<level_node<Symbol>>(leaves.begin(), leaves.end());
    }

    //! Reconstructs the final Hu-Tucker tree based on the level-tagged nodes.
    template<class Symbol = char>
    node_ptr<Symbol> reconstruct(std::list<level_node<Symbol>>& nodes)
    {
        EXPECTS(nodes.size() > 1);
        std::list<level_node<Symbol>> stack;
        std::list<Symbol> propagated_symbols;
        while (true) {
            if (stack.size() > 1
                && stack.back().level == std::next(stack.rbegin())->level) {
                auto right = stack.back();
                stack.pop_back();
                auto left = stack.back();
                stack.pop_back();
                Symbol rs = propagated_symbols.back();
                propagated_symbols.pop_back();
                Symbol ls = propagated_symbols.back();
                propagated_symbols.pop_back();
                level_node<Symbol> joined = {left.level - 1,
                    join_nodes_with_symbol(left.node, right.node, ls)};
                if (joined.level == 0) {
                    return joined.node;
                }
                stack.push_back(joined);
                propagated_symbols.push_back(rs);
            } else if (nodes.size() > 0) {
                auto next = nodes.front();
                nodes.pop_front();
                stack.push_back(next);
                propagated_symbols.push_back(next.node->symbol.value());
            } else {
                throw std::runtime_error(
                    "wrong level alignment: check first-phase");
            }
        }
    }

    //! Returns an immutable compact version of the same tree.
    //template<class Symbol = char>
    alphabetical_bst<char, uint16_t, std::vector<char>>
    compact(node_ptr<char> root)
    {
        using alphabetical_bst =
            alphabetical_bst<char, uint16_t, std::vector<char>>;
        using node = typename alphabetical_bst::node;
        std::size_t node_size = alphabetical_bst::node_size;

        std::vector<node> compact_nodes;
        std::list<node_ptr<char>> queue;
        queue.push_back(root);

        auto is_leaf = [](node_ptr<char> n) { return n->left == nullptr; };
        auto calc_ptr = [&compact_nodes, &queue, node_size, is_leaf](auto n) {
            uint16_t ptr;
            if (is_leaf(n)) {
                ptr = static_cast<uint16_t>((unsigned char)n->symbol.value());
            } else {
                ptr = alphabetical_bst::symbol_bound
                    + (compact_nodes.size() + queue.size() + 1) * node_size;
                queue.push_back(n);
            }
            return ptr;
        };

        while (!queue.empty()) {
            auto n = queue.front();
            queue.pop_front();
            auto left_ptr = calc_ptr(n->left);
            auto right_ptr = calc_ptr(n->right);
            compact_nodes.push_back(
                node(n->symbol.value(), left_ptr, right_ptr));
        }

        std::vector<char> mem;
        for (const auto& node : compact_nodes) {
            mem.insert(mem.end(), node.bytes, node.bytes + node_size);
        }
        return alphabetical_bst(mem);
    }

};  // namespace coding::hutucker

//! Hu-Tucker codec.
/*!
 * @tparam Symbol           the type of encoded symbols (`char` by default)
 * @tparam MemoryContainer  where the ABST memory is read from; could be, e.g.,
 *                          a vector or a memory mapped file
 *
 * See http://www-math.mit.edu/~shor/PAM/hu-tucker_algorithm.html for a
 * description of the algorithm and sturctures.
 */
template<class Symbol = char, class MemoryContainer = std::vector<char>>
class hutucker_codec {
public:
    using symbol_type = Symbol;
    using buffer_type = MemoryContainer;
    using self_type = hutucker_codec<symbol_type, buffer_type>;
    static constexpr std::size_t symbol_count = 1 << (sizeof(symbol_type) * 8);

private:
    alphabetical_bst<symbol_type, uint16_t, buffer_type> abst_;

public:
    hutucker_codec(const self_type&) = default;
    hutucker_codec(self_type&&) = default;
    //! Constructs a codec from an existing ABST.
    hutucker_codec(
        alphabetical_bst<symbol_type, uint16_t, buffer_type> abst)
        : abst_(abst)
    {}

    //! Constructs a codec from a vector of all symbols' frequencies.
    /*!
     * Note that this constructor is only available whenever `buffer_type`
     * is a `std::vector<char>`.
     */
    template<class = enable_if_equal<buffer_type, std::vector<char>>>
    hutucker_codec(const std::vector<std::size_t>& frequencies)
    {
        EXPECTS(frequencies.size() == symbol_count);
        auto initial = coding::huffman::init_nodes(frequencies);
        auto initial_tree = coding::hutucker::build_tree(initial);
        auto tagged_leaves = coding::hutucker::tag_leaves(initial_tree);
        auto tree = coding::hutucker::reconstruct(tagged_leaves);
        abst_ = coding::hutucker::compact(tree);
    }

    //! Returns a dynamic bitset representing the encoded word.
    /*!
     * @tparam SymbolIterator   an input iterator of `symbol_type`s
     * @param first     an iterator pointing at the first symbol to encode
     * @param last      an iterator pointing after the last symbol to encode
     */
    template<class SymbolIterator>
    boost::dynamic_bitset<unsigned char>
    encode(SymbolIterator first, SymbolIterator last) const
    {
        boost::dynamic_bitset<unsigned char> encoded;
        while (first != last) {
            abst_.encode(*first, encoded);
            ++first;
        }
        return encoded;
    }

    //! Returns a dynamic bitset representing the encoded string.
    /*!
     * Available only when `symbol_type = char`. Then, equivalent to
     * `encode(word.begin(), word.end())`.
     */
    template<class = enable_if_equal<symbol_type, char>>
    boost::dynamic_bitset<unsigned char> encode(const std::string& word) const
    {
        return encode(word.begin(), word.end());
    }

    //! Encodes the entire input stream to the sink.
    /*!
     * @param source    input stream containing `symbol_type`s
     * @param sink      output stream writing encoded bits
     */
    template<class SymbolInputStream, class BitOutputStream>
    std::size_t encode(SymbolInputStream& source, BitOutputStream& sink) const
    {
        symbol_type symbol;
        std::size_t read_symbols = 0;
        while (source.read(&symbol, sizeof(symbol_type))) {
            abst_.encode(symbol, sink);
            ++read_symbols;
        }
        return read_symbols;
    }

    template<class SymbolIterator, class BitOutputStream>
    std::ptrdiff_t encode(
        SymbolIterator first, SymbolIterator last, BitOutputStream& sink) const
    {
        auto it = first;
        while (it != last) {
            abst_.encode(*it, sink);
            ++it;
        }
        return std::distance(first, last);
    }

    template<class SymbolIterator, class BitOutputStream>
    std::ptrdiff_t encode(const std::string& word, BitOutputStream& sink) const
    {
        return encode(word.begin(), word.end(), sink);
    }

    //! Decodes `n` symbols from a bitset and writes to an output stream.
    /*!
     * @param source    a bitset to decode
     * @param sink      an output stream to write the decoded symbols
     * @param n         the number of symbols to decode
     */
    std::size_t decode(const boost::dynamic_bitset<unsigned char>& source,
        std::ostream& sink,
        std::size_t n) const
    {
        for (std::size_t idx = 0; idx < n; ++idx) {
            symbol_type symbol = abst_.decode(source);
            sink.write(&symbol, sizeof(symbol));
        }
        return n;
    }

    //! Decodes `n` symbols from a bit input stream and writes them to an output
    //! stream.
    /*!
     * @param source    a bit input stream
     * @param sink      an output stream to write the decoded symbols
     * @param n         the number of symbols to decode
     */
    template<class BitInputStream>
    std::size_t
    decode(BitInputStream& source, std::ostream& sink, std::size_t n) const
    {
        for (std::size_t idx = 0; idx < n; ++idx) {
            symbol_type symbol = abst_.decode(source);
            sink.write(&symbol, sizeof(symbol));
        }
        return n;
    }

    //! Returns the tree used to encode and decode symbols.
    const alphabetical_bst<symbol_type, uint16_t, buffer_type>& tree() const
    {
        return abst_;
    }
};

};  // namespace irk::coding
