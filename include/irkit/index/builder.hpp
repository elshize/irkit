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

//! \file builder.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <algorithm>
#include <iostream>
#include <optional>
#include <unordered_map>
#include <vector>
#include "irkit/coding.hpp"
#include "irkit/coding/varbyte.hpp"
#include "irkit/compacttable.hpp"

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

    struct doc_freq_pair {
        document_type doc;
        frequency_type freq;
        bool operator==(const doc_freq_pair& rhs) const
        {
            return doc == rhs.doc && freq == rhs.freq;
        }
    };

    document_type current_doc_;
    std::optional<std::vector<term_type>> sorted_terms_;
    std::vector<std::vector<doc_freq_pair>> postings_;
    std::vector<std::size_t> term_occurrences_;
    std::unordered_map<term_type, term_id_type> term_map_;

public:
    index_builder() : current_doc_(0) {}

    //! Initiates a new document with an incremented ID.
    void add_document() { current_doc_++; }

    //! Initiates a new document with the given ID.
    void add_document(document_type doc) { current_doc_ = doc; }

    //! Adds a term to the current document.
    void add_term(const term_type& term)
    {
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
        std::vector<std::size_t> term_occurrences;
        for (const std::string& term : sorted_terms_.value()) {
            term_id_type term_id = term_map_[term];
            term_map_[term] = postings.size();
            postings.push_back(std::move(postings_[term_id]));
            term_occurrences.push_back(std::move(term_occurrences_[term_id]));
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
        if (sorted_terms_ == std::nullopt) {
            sort_terms();
        }
        std::size_t offset = 0;
        //std::size_t offset_delta = 0;
        std::vector<std::size_t> offsets;
        for (auto& term : sorted_terms_.value()) {
            offsets.push_back(offset);
            term_id_type term_id = term_map_[term];
            document_type last = 0;
            std::vector<document_type> deltas;
            for (const auto& posting : postings_[term_id]) {
                deltas.push_back(posting.doc - last);
                last = posting.doc;
            }
            auto encoded =
                coding::encode<coding::varbyte_codec<frequency_type>>(deltas);
            out.write(reinterpret_cast<char*>(&encoded[0]), encoded.size());
            offset += encoded.size();
        }
        offset_table<> offset_table = build_offset_table<>(offsets);
        off << offset_table;
    }

    //! Writes term-document frequencies (tf).
    void write_document_counts(std::ostream& out, std::ostream& off)
    {
        if (sorted_terms_ == std::nullopt) {
            sort_terms();
        }
        std::size_t offset = 0;
        std::vector<std::size_t> offsets;
        for (auto& term : sorted_terms_.value()) {
            offsets.push_back(offset);
            term_id_type term_id = term_map_[term];
            auto freq_fn = [](const auto& posting) { return posting.freq; };
            auto encoded =
                coding::encode_fn<coding::varbyte_codec<frequency_type>>(
                    postings_[term_id], freq_fn);
            out.write(reinterpret_cast<char*>(&encoded[0]), encoded.size());
            offset += encoded.size();
        }
        offset_table<> offset_table = build_offset_table<>(offsets);
        off << offset_table;
    }

    //! Writes document frequencies (df).
    void write_document_frequencies(std::ostream& out)
    {
        if (sorted_terms_ == std::nullopt) {
            sort_terms();
        }
        std::vector<frequency_type> dfs;
        for (auto& term : sorted_terms_.value()) {
            term_id_type term_id = term_map_[term];
            dfs.push_back(document_frequency(term_id));
        }
        auto compact_term_dfs = irk::build_compact_table<frequency_type>(dfs);
        out << compact_term_dfs;
    }
};

using default_index_builder =
    index_builder<std::uint32_t, std::string, std::uint32_t, std::uint32_t>;

};  // namespace irk
