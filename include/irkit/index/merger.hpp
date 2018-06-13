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

#include <memory>
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
    using index_type = v2::inverted_index_view;

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
    std::vector<irk::v2::inverted_index_mapped_data_source> sources_;
    std::vector<entry> heap_;
    std::ofstream terms_out_;
    std::ofstream doc_ids_;
    std::ofstream doc_counts_;
    std::vector<std::size_t> doc_ids_off_;
    std::vector<std::size_t> doc_counts_off_;
    std::vector<frequency_type> term_dfs_;
    std::size_t doc_offset_;
    std::size_t count_offset_;
    long block_size_;
    any_codec<document_type> document_codec_;
    any_codec<frequency_type> frequency_codec_;

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
        any_codec<document_type> document_codec,
        any_codec<frequency_type> frequency_codec,
        long block_size,
        bool skip_unique = false)
        : target_dir_(target_dir),
          document_codec_(document_codec),
          frequency_codec_(frequency_codec),
          block_size_(block_size),
          skip_unique_(skip_unique)
    {
        for (fs::path index_dir : indices) {
            std::cout << "Loading index " << index_dir << std::endl;
            sources_.emplace_back(index_dir);
            indices_.emplace_back(
                &sources_.back(), document_codec_, frequency_codec_);
        }
        terms_out_.open(index::terms_path(target_dir).c_str());
        doc_ids_.open(index::doc_ids_path(target_dir).c_str());
        doc_counts_.open(index::doc_counts_path(target_dir).c_str());
        doc_offset_ = 0;
        count_offset_ = 0;
    }

    void merge_term(std::vector<entry>& indices)
    {
        // Write the term.
        terms_out_ << indices.front().current_term() << std::endl;
        std::cout << "Merging term: " << indices.front().current_term() << std::endl;

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
            std::cout << "Reading postings" << std::endl;
            auto pr = e.index->postings(e.current_term_id);
            for (const auto& p : pr) {
                doc_ids.push_back(p.document() + e.shift);
                doc_counts.push_back(p.payload());
                std::cout << doc_ids.back() << " " << doc_counts.back() << std::endl;
            }
        }

        // Accumulate the term's document frequency.
        term_dfs_.push_back(doc_ids.size());

        // Accumulate offsets.
        doc_ids_off_.push_back(doc_offset_);
        doc_counts_off_.push_back(count_offset_);

        // Write documents and counts.
        index::block_list_builder<document_type, true> doc_list_builder(
            block_size_, document_codec_);
        for (const auto& doc : doc_ids) { doc_list_builder.add(doc); }
        doc_offset_ += doc_list_builder.write(doc_ids_);

        index::block_list_builder<frequency_type, false> count_list_builder(
            block_size_, frequency_codec_);
        for (const auto& count : doc_counts) { count_list_builder.add(count); }
        count_offset_ += count_list_builder.write(doc_counts_);
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
        io::dump(build_offset_table<>(doc_ids_off_),
            index::doc_ids_off_path(target_dir_));
        io::dump(build_offset_table<>(doc_counts_off_),
            index::doc_counts_off_path(target_dir_));

        // Write term dfs.
        auto compact_term_dfs =
            irk::build_compact_table<frequency_type>(term_dfs_);
        irk::io::dump(compact_term_dfs, index::term_doc_freq_path(target_dir_));

        // Close other streams.
        terms_out_.close();
        doc_ids_.close();
        doc_counts_.close();
    }

    void merge_titles()
    {
        std::ofstream tout(index::titles_path(target_dir_).c_str());
        for (const auto& index : indices_) {
            for (const auto& title : index.titles())
            { tout << title << std::endl; }
        }
        tout.close();
    }
};

using default_index_merger = index_merger<long, std::string, long, long>;

};  // namespace irk
