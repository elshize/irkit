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

//! \file merger.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <string>
#include "irkit/index.hpp"

namespace irk {

template<class Doc = std::size_t,
    class Term = std::string,
    class TermId = std::size_t,
    class Freq = std::size_t>
class index_merger {
public:
    using document_type = Doc;
    using term_type = Term;
    using term_id_type = TermId;
    using frequency_type = Freq;
    using index_type =
        inverted_index<document_type, term_type, term_id_type, frequency_type>;

private:
    struct entry {
        term_id_type current_term_id;
        const index_type* index;
        document_type shift;
        std::string current_term() const
        {
            return index->term(current_term_id);
        }
        bool operator<(const entry& rhs) const
        {
            return current_term() > rhs.current_term();
        }
        bool operator>(const entry& rhs) const
        {
            return current_term() < rhs.current_term();
        }
        bool operator==(const entry& rhs) const
        {
            return current_term() == rhs.current_term();
        }
        bool operator!=(const entry& rhs) const
        {
            return current_term() != rhs.current_term();
        }
    };
    fs::path target_dir_;
    bool skip_unique_;
    std::vector<index_type> indices_;
    std::vector<entry> heap_;
    std::ofstream terms_out_;
    std::ofstream doc_ids_;
    std::ofstream doc_counts_;
    std::vector<std::size_t> doc_ids_off_;
    std::vector<std::size_t> doc_counts_off_;
    std::vector<frequency_type> term_dfs_;
    std::size_t doc_offset_;
    std::size_t count_offset_;

    std::vector<entry> indices_with_next_term()
    {
        std::vector<entry> result;
        std::string next_term = heap_.front().current_term();
        while (!heap_.empty() && heap_.front().current_term() == next_term) {
            std::pop_heap(heap_.begin(), heap_.end());
            result.push_back(heap_.back());
            heap_.pop_back();
        }
        return result;
    }

public:
    index_merger(fs::path target_dir,
        std::vector<fs::path> indices,
        bool skip_unique = false)
        : target_dir_(target_dir), skip_unique_(skip_unique)
    {
        for (fs::path index_dir : indices) {
            std::cout << "Loading index " << index_dir << std::endl;
            indices_.emplace_back(index_dir, false, true);
        }
        terms_out_.open(index::terms_path(target_dir));
        doc_ids_.open(index::doc_ids_path(target_dir));
        doc_counts_.open(index::doc_counts_path(target_dir));
        doc_offset_ = 0;
        count_offset_ = 0;
    }

    void merge_term(std::vector<entry>& indices)
    {
        if (skip_unique_ && indices.size() == 1) {
            auto pr = indices.front().index->posting_range(
                indices.front().current_term_id, score::count_scorer{});
            if (pr.size() == 1) {
                std::cerr << "Skipping unique term "
                          << indices.front().current_term() << std::endl;
                return;
            }
        }
        std::cerr << "Merging term " << term_dfs_.size() << " ("
                  << indices.front().current_term() << ")" << std::endl;

        // Write the term.
        terms_out_ << indices.front().current_term() << std::endl;

        // Sort by shift, i.e., effectively document IDs.
        std::sort(indices.begin(),
            indices.end(),
            [](const entry& lhs, const entry& rhs) {
                return lhs.shift < rhs.shift;
            });

        // Merge the posting lists.
        std::vector<document_type> doc_ids;
        std::vector<frequency_type> doc_counts;
        for (const entry& e : indices) {
            auto pr = e.index->posting_range(
                e.current_term_id, score::count_scorer{});
            for (auto p : pr) {
                doc_ids.push_back(p.doc + e.shift);
                doc_counts.push_back(p.score);
            }
        }

        // Accumulate the term's document frequency.
        term_dfs_.push_back(doc_ids.size());

        // Accumulate offsets.
        doc_ids_off_.push_back(doc_offset_);
        doc_counts_off_.push_back(count_offset_);

        // Write documents and counts.
        auto encoded_ids =
            irk::coding::encode_delta<varbyte_codec<document_type>>(doc_ids);
        doc_ids_.write(encoded_ids.data(), encoded_ids.size());
        auto encoded_counts =
            irk::coding::encode<varbyte_codec<frequency_type>>(doc_counts);
        doc_counts_.write(encoded_counts.data(), encoded_counts.size());

        // Calc new offsets.
        doc_offset_ += encoded_ids.size();
        count_offset_ += encoded_counts.size();
    }

    void merge_terms()
    {
        // Initialize heap: everyone starts with the first term.
        document_type shift(0);
        for (const index_type& index : indices_) {
            heap_.push_back({0, &index, shift});
            shift += index.collection_size();
        }

        while (!heap_.empty()) {
            std::vector<entry> indices_to_merge = indices_with_next_term();
            merge_term(indices_to_merge);
            for (const entry& e : indices_to_merge) {
                if (std::size_t(e.current_term_id + 1)
                    < e.index->term_count()) {
                    heap_.push_back(
                        entry{e.current_term_id + 1, e.index, e.shift});
                    std::push_heap(heap_.begin(), heap_.end());
                }
            }
        }

        // Write offsets
        io::dump(
            offset_table<>(doc_ids_off_), index::doc_ids_off_path(target_dir_));
        io::dump(offset_table<>(doc_counts_off_),
            index::doc_counts_off_path(target_dir_));

        // Write term dfs.
        std::vector<char> encoded_dfs =
            irk::coding::encode_delta<varbyte_codec<frequency_type>>(term_dfs_);
        std::ofstream dfs(index::term_doc_freq_path(target_dir_));
        dfs.write(encoded_dfs.data(), encoded_dfs.size());
        dfs.close();

        // Close other streams.
        terms_out_.close();
        doc_ids_.close();
        doc_counts_.close();
    }

    void merge_titles()
    {
        std::ofstream tout(index::titles_path(target_dir_));
        for (const auto& index : indices_) {
            for (const auto& title : index.titles()) {
                tout << title << std::endl;
            }
        }
        tout.close();
    }
};

using default_index_merger =
    index_merger<std::uint32_t, std::string, std::uint32_t, std::uint32_t>;

};  // namespace irk
