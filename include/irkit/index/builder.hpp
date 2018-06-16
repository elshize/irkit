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

#include <algorithm>
#include <iostream>
#include <optional>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include <irkit/coding.hpp>
#include <irkit/coding/varbyte.hpp>
#include <irkit/compacttable.hpp>
#include <irkit/index/block_inverted_list.hpp>

namespace irk {

template<class Doc = std::size_t,
    class Term = std::string,
    class TermId = std::size_t,
    class Freq = std::size_t>
class index_builder {
public:
    using document_type = Doc;
    using term_type = Term;
    using term_id_type = TermId;
    using frequency_type = Freq;

private:
    struct doc_freq_pair {
        document_type doc;
        frequency_type freq;
        bool operator==(const doc_freq_pair& rhs) const
        {
            return doc == rhs.doc && freq == rhs.freq;
        }
    };

    int block_size_;
    document_type current_doc_;
    long all_occurrences_;
    std::optional<std::vector<term_type>> sorted_terms_;
    std::vector<std::vector<doc_freq_pair>> postings_;
    std::vector<frequency_type> term_occurrences_;
    std::vector<frequency_type> document_sizes_;
    std::unordered_map<term_type, term_id_type> term_map_;

public:
    index_builder(int block_size = 64)
        : block_size_(block_size), current_doc_(0), all_occurrences_(0)
    {}

    //! Initiates a new document with an incremented ID.
    void add_document() { add_document(current_doc_ + 1); }

    //! Initiates a new document with the given ID.
    void add_document(document_type doc)
    {
        current_doc_ = doc;
        document_sizes_.push_back(0);
    }

    //! Adds a term to the current document.
    void add_term(const term_type& term)
    {
        ++all_occurrences_;
        ++document_sizes_[current_doc_];
        auto ti = term_map_.find(term);
        if (ti != term_map_.end()) {
            term_id_type term_id = ti->second;
            if (postings_[term_id].back().doc == current_doc_) {
                postings_[term_id].back().freq++;
            } else {
                postings_[term_id].push_back({current_doc_, 1});
            }
            term_occurrences_[term_id]++;
        } else {
            term_id_type term_id = term_map_.size();
            term_map_[term] = term_id;
            postings_.push_back({{current_doc_, 1}});
            term_occurrences_.push_back(1);
        }
    }

    //! Returns the document frequency of the given term.
    frequency_type document_frequency(term_id_type term_id)
    {
        return postings_[term_id].size();
    }

    //! Returns the number of distinct terms.
    std::size_t term_count() { return term_map_.size(); }

    //! Returns the number of the accumulated documents.
    std::size_t collection_size() { return current_doc_ + 1; }

    //! Sorts the terms, and all related structures, lexicographically.
    void sort_terms()
    {
        sorted_terms_ = std::vector<term_type>();
        for (const auto& term_entry : term_map_) {
            const term_type& term = term_entry.first;
            sorted_terms_->push_back(term);
        }
        std::sort(sorted_terms_->begin(), sorted_terms_->end());

        // Create new related structures and swap.
        std::vector<std::vector<doc_freq_pair>> postings;
        std::vector<frequency_type> term_occurrences;
        for (const std::string& term : sorted_terms_.value()) {
            term_id_type term_id = term_map_[term];
            term_map_[term] = postings.size();
            postings.push_back(std::move(postings_[term_id]));
            term_occurrences.push_back(term_occurrences_[term_id]);
        }
        postings_ = std::move(postings);
        term_occurrences_ = std::move(term_occurrences);
    }

    //! Writes new-line-delimited sorted list of terms.
    void write_terms(std::ostream& out)
    {
        if (sorted_terms_ == std::nullopt) {
            sort_terms();
        }
        for (auto& term : sorted_terms_.value()) {
            out << term << std::endl;
        }
    }

    //! Writes document IDs
    void write_document_ids(std::ostream& out, std::ostream& off)
    {
        if (sorted_terms_ == std::nullopt) { sort_terms(); }
        std::size_t offset = 0;
        std::vector<std::size_t> offsets;
        for (auto& term : sorted_terms_.value())
        {
            offsets.push_back(offset);
            term_id_type term_id = term_map_[term];
            index::block_list_builder<document_type, true> list_builder(
                block_size_, varbyte_codec<document_type>{});
            for (const auto& posting : postings_[term_id])
            { list_builder.add(posting.doc); }
            offset += list_builder.write(out);
        }
        offset_table<> offset_table = build_offset_table<>(offsets);
        off << offset_table;
    }

    //! Writes term-document frequencies (tf).
    void write_document_counts(std::ostream& out, std::ostream& off)
    {
        if (sorted_terms_ == std::nullopt) { sort_terms(); }
        std::size_t offset = 0;
        std::vector<std::size_t> offsets;
        for (auto& term : sorted_terms_.value()) {
            offsets.push_back(offset);
            term_id_type term_id = term_map_[term];
            index::block_list_builder<frequency_type, false> list_builder(
                block_size_, varbyte_codec<frequency_type>{});
            for (const auto& posting : postings_[term_id])
            { list_builder.add(posting.freq); }
            offset += list_builder.write(out);
        }
        offset_table<> offset_table = build_offset_table<>(offsets);
        off << offset_table;
    }

    //! Writes document frequencies (df).
    void write_document_frequencies(std::ostream& out)
    {
        if (sorted_terms_ == std::nullopt) { sort_terms(); }
        std::vector<frequency_type> dfs;
        for (auto& term : sorted_terms_.value())
        {
            term_id_type term_id = term_map_[term];
            dfs.push_back(document_frequency(term_id));
        }
        auto compact_term_dfs = irk::build_compact_table<frequency_type>(dfs);
        out << compact_term_dfs;
    }

    //! Writes document sizes.
    void write_document_sizes(std::ostream& out) const
    {
        auto compact_sizes = irk::build_compact_table<frequency_type>(
            document_sizes_);
        out << compact_sizes;
    }

    //! Writes document sizes.
    void write_term_occurrences(std::ostream& out) const
    {
        auto compact_table = irk::build_compact_table<frequency_type>(
            term_occurrences_);
        out << compact_table;
    }

    //! Writes properties to the output stream.
    void write_properties(std::ostream& out) const
    {
        long sizes_sum = std::accumulate(
            document_sizes_.begin(), document_sizes_.end(), 0);
        nlohmann::json j = {
            {"documents", current_doc_ + 1},
            {"occurrences", all_occurrences_},
            {"skip_block_size", block_size_},
            {"avg_document_size", (double)sizes_sum / document_sizes_.size()}
        };
        out << std::setw(4) << j << std::endl;
    }
};

using default_index_builder =
    index_builder<std::uint32_t, std::string, std::uint32_t, std::uint32_t>;

};  // namespace irk
