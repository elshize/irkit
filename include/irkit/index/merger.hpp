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

#include <memory>
#include <string>

#include <boost/log/trivial.hpp>

#include <irkit/index.hpp>
#include <irkit/index/source.hpp>
#include <irkit/index/types.hpp>

namespace irk {

template<class Term = std::string,
    class TermId = std::size_t,
    class Freq = std::size_t>
class index_merger {
public:
    using document_type = irk::index::document_t;
    using term_type = Term;
    using term_id_type = TermId;
    using frequency_type = Freq;
    using index_type = inverted_index_view;

private:
    class entry {
    private:
        int index_id_;
        term_id_type current_term_id_;
        const index_type* index_;
        document_type shift_;
        std::string current_term_;

    public:
        entry() = default;
        entry(int index_id,
            term_id_type current_term_id,
            const index_type* index,
            document_type shift,
            std::string current_term)
            : index_id_(index_id),
              current_term_id_(current_term_id),
              index_(index),
              shift_(shift),
              current_term_(current_term)
        {}
        entry(const entry& other) = default;
        entry& operator=(const entry& other) = default;

        int index_id() const { return index_id_; }
        const std::string& current_term() const { return current_term_; }
        term_id_type current_term_id() const { return current_term_id_; }
        document_type shift() const { return shift_; }
        const index_type* index() const { return index_; }

        bool operator<(const entry& rhs) const
        { return current_term() > rhs.current_term(); }
        bool operator>(const entry& rhs) const
        { return current_term() < rhs.current_term(); }
        bool operator==(const entry& rhs) const
        { return current_term() == rhs.current_term(); }
        bool operator!=(const entry& rhs) const
        { return current_term() != rhs.current_term(); }

        auto term_count() const { return index_->term_count(); }
        auto postings() const { return index_->postings(current_term_id_); }
        auto documents() const { return index_->documents(current_term_id_); }
        auto frequencies() const
        { return index_->frequencies(current_term_id_); }
    };
    fs::path target_dir_;
    bool skip_unique_;
    std::vector<index_type> indices_;
    std::vector<irk::inverted_index_mapped_data_source> sources_;
    std::vector<entry> heap_;
    std::ofstream terms_out_;
    std::ofstream doc_ids_;
    std::ofstream doc_counts_;
    std::vector<std::size_t> doc_ids_off_;
    std::vector<std::size_t> doc_counts_off_;
    std::vector<frequency_type> term_dfs_;
    index::offset_t doc_offset_;
    index::offset_t count_offset_;
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
            sources_.emplace_back(index_dir);
            indices_.emplace_back(&sources_.back(),
                document_codec_,
                frequency_codec_,
                frequency_codec_);
        }
        terms_out_.open(index::terms_path(target_dir).c_str());
        doc_ids_.open(index::doc_ids_path(target_dir).c_str());
        doc_counts_.open(index::doc_counts_path(target_dir).c_str());
        doc_offset_ = 0;
        count_offset_ = 0;
    }

    long copy_term(const entry& index_entry)
    {
        auto term_id = index_entry.current_term_id();
        doc_offset_ += index_entry.index()->copy_document_list(
            term_id, doc_ids_);
        count_offset_ += index_entry.index()->copy_frequency_list(
            term_id, doc_counts_);
        term_dfs_.push_back(index_entry.index()->tdf(term_id));
        return index_entry.index()->term_occurrences(term_id);
    }

    long merge_term(std::vector<entry>& indices)
    {
        // Write the term.
        std::string term = indices.front().current_term();
        terms_out_ << term << std::endl;

        // Accumulate offsets.
        doc_ids_off_.push_back(doc_offset_);
        doc_counts_off_.push_back(count_offset_);

        if (indices.size() == 1) { return copy_term(indices.front()); }

        // Sort by shift, i.e., effectively document IDs.
        std::sort(indices.begin(),
            indices.end(),
            [](const entry& lhs, const entry& rhs) {
                return lhs.shift() < rhs.shift();
            });

        // Merge the posting lists.
        long occurrences = 0;
        std::vector<document_type> doc_ids;
        std::vector<frequency_type> doc_counts;
        for (const entry& e : indices) {
            occurrences += e.index()->term_occurrences(e.current_term_id());
            auto d = e.documents();
            auto f = e.frequencies();
            doc_ids.insert(doc_ids.end(), d.begin(), d.end());
            doc_counts.insert(doc_counts.end(), f.begin(), f.end());
        }

        // Accumulate the term's document frequency.
        term_dfs_.push_back(doc_ids.size());

        // Write documents and counts.
        index::block_list_builder<document_type, true> doc_list_builder(
            block_size_, document_codec_);
        for (const auto& doc : doc_ids) { doc_list_builder.add(doc); }
        doc_offset_ += doc_list_builder.write(doc_ids_);

        index::block_list_builder<frequency_type, false> count_list_builder(
            block_size_, frequency_codec_);
        for (const auto& count : doc_counts) { count_list_builder.add(count); }
        count_offset_ += count_list_builder.write(doc_counts_);

        return occurrences;
    }


    long merge_terms()
    {
        // Initialize heap: everyone starts with the first term.
        std::vector<std::ifstream*> term_streams;
        document_type shift(0);
        int index_num = 0;
        BOOST_LOG_TRIVIAL(debug) << "Initializing heap..." << std::flush;
        for (const index_type& index : indices_) {
            term_streams.push_back(new std::ifstream(
                (sources_[index_num].dir() / "terms.txt").c_str()));
            std::string current_term;
            *term_streams.back() >> current_term;
            heap_.emplace_back(index_num, 0, &index, shift, current_term);
            shift += index.collection_size();
            index_num++;
        }

        BOOST_LOG_TRIVIAL(debug) << "Initialized.";
        long all_occurrences = 0;
        std::vector<long> occurrences;
        long term_id = 0;
        while (!heap_.empty()) {
            std::vector<entry> indices_to_merge = indices_with_next_term();
            BOOST_LOG_TRIVIAL(debug) << "Merging term #" << term_id++
                                     << " from " << indices_to_merge.size()
                                     << " indices" << std::flush;
            occurrences.push_back(merge_term(indices_to_merge));
            all_occurrences += occurrences.back();
            for (entry& e : indices_to_merge) {
                if (std::size_t(e.current_term_id() + 1) < e.term_count())
                {
                    std::string current_term;
                    *term_streams[e.index_id()] >> current_term;
                    heap_.emplace_back(e.index_id(),
                        e.current_term_id() + 1,
                        e.index(),
                        e.shift(),
                        current_term);
                    std::push_heap(heap_.begin(), heap_.end());
                }
            }
        }
        // Write occurrences
        io::dump(build_compact_table<>(occurrences),
            index::term_occurrences_path(target_dir_));

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
        for (int idx = 0; idx < term_streams.size(); idx++) {
            delete term_streams[idx];
        }
        terms_out_.close();
        doc_ids_.close();
        doc_counts_.close();

        return all_occurrences;
    }

    void merge_titles()
    {
        std::ofstream tout(index::titles_path(target_dir_).c_str());
        for (const auto& index : indices_) {
            for (const std::string& title : index.titles())
            { tout << title << std::endl; }
        }
        tout.close();
    }

    std::pair<long, double> merge_sizes()
    {
        std::vector<long> sizes;
        std::ofstream sout(index::doc_sizes_path(target_dir_).c_str());
        long sizes_sum = 0;
        for (const auto& index : indices_) {
            for (long doc = 0; doc < index.collection_size(); ++doc)
            {
                long size = index.document_size(doc);
                sizes.push_back(size);
                sizes_sum += size;
            }
        }
        irk::compact_table<long> size_table = irk::build_compact_table(sizes, false);
        sout << size_table;
        return std::make_pair(sizes.size(), (double)sizes_sum / sizes.size());
    }

    void write_properties(long documents, long occurrences, double avg_doc_size)
    {
        std::ofstream out(index::properties_path(target_dir_).c_str());
        nlohmann::json j = {
            {"documents", documents},
            {"occurrences", occurrences},
            {"skip_block_size", block_size_},
            {"avg_document_size", avg_doc_size}
        };
        out << std::setw(4) << j << std::endl;
    }

    void merge()
    {
        BOOST_LOG_TRIVIAL(info) << "Merging titles...";
        merge_titles();
        BOOST_LOG_TRIVIAL(info) << "Merging terms...";
        long occurrences = merge_terms();
        BOOST_LOG_TRIVIAL(info) << "Merging sizes...";
        auto [documents, avg_doc_size] = merge_sizes();
        BOOST_LOG_TRIVIAL(info) << "Writing properties...";
        write_properties(documents, occurrences, avg_doc_size);
    }
};

using default_index_merger = index_merger<std::string, long, long>;

};  // namespace irk
