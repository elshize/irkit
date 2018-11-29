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
#include <numeric>
#include <string>

#include <pstl/algorithm>
#include <pstl/execution>
#include <pstl/numeric>
#include <spdlog/spdlog.h>

#include <irkit/index.hpp>
#include <irkit/index/source.hpp>
#include <irkit/index/types.hpp>
#include <irkit/list/standard_block_list.hpp>

namespace irk {

using std::int32_t;
using std::uint32_t;
using std::int64_t;
using std::uint64_t;

namespace index::detail {

    template<class IndexRng>
    std::tuple<int32_t, double, int32_t>
    merge_sizes(const IndexRng& indices, std::ostream& sout)
    {
        std::vector<int> range(indices.size());
        std::iota(range.begin(), range.end(), 0);
        std::vector<int64_t> partial_count(indices.size());
        std::transform(
            indices.begin(),
            indices.end(),
            partial_count.begin(),
            [](const auto& index) { return index.collection_size(); });
        std::partial_sum(
            partial_count.begin(), partial_count.end(), partial_count.begin());
        int32_t document_count = partial_count.back();

        std::vector<int32_t> sizes(document_count);
        std::for_each(
            std::execution::par_unseq,
            range.begin(),
            range.end(),
            [&](auto idx) {
                auto part_sizes = indices[idx].document_sizes();
                auto offset = partial_count[idx] - part_sizes.size();
                std::copy(
                    part_sizes.begin(),
                    part_sizes.end(),
                    std::next(sizes.begin(), offset));
            });

        int32_t max_doc_size = *std::max_element(
            std::execution::par_unseq, sizes.begin(), sizes.end());
        int64_t sum_doc_size = std::reduce(
            std::execution::par_unseq, sizes.begin(), sizes.end(), int64_t(0));
        double avg_doc_size =
            static_cast<double>(sum_doc_size) / document_count;

        sout << irk::build_compact_table(sizes, false);
        return std::make_tuple(document_count, avg_doc_size, max_doc_size);
    }

}  // namespace index::detail

template<class DocumentCodec = irk::stream_vbyte_codec<index::document_t>,
    class FrequencyCodec = irk::stream_vbyte_codec<index::frequency_t>>
class basic_index_merger {
public:
    using document_type = index::document_t;
    using term_type = index::term_t;
    using term_id_type = index::term_id_t;
    using frequency_type = index::frequency_t;
    using document_codec_type = DocumentCodec;
    using frequency_codec_type = FrequencyCodec;
    using index_type = basic_inverted_index_view<document_codec_type>;
    using source_type = inverted_index_mapped_data_source;

private:
    class entry {
    private:
        int index_id_ = 0;
        term_id_type current_term_id_ = 0;
        const index_type* index_ = nullptr;
        document_type shift_ = 0;
        std::string current_term_{};

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
              current_term_(std::move(current_term))
        {}
        entry(const entry&) = default;
        entry& operator=(const entry&) = default;
        entry(entry&&) noexcept = default;
        entry& operator=(entry&&) noexcept = default;
        ~entry() = default;

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
    std::vector<source_type> sources_;
    std::vector<entry> heap_;
    std::ofstream terms_out_;
    std::ofstream doc_ids_;
    std::ofstream doc_counts_;
    std::vector<std::size_t> doc_ids_off_;
    std::vector<std::size_t> doc_counts_off_;
    std::vector<frequency_type> term_dfs_;
    index::offset_t doc_offset_;
    index::offset_t count_offset_;
    int block_size_;
    document_codec_type document_codec_;
    frequency_codec_type frequency_codec_;

    std::vector<entry> indices_with_next_term()
    {
        std::vector<entry> result;
        std::string next_term = heap_.front().current_term();
        while (not heap_.empty() && heap_.front().current_term() == next_term)
        {
            std::pop_heap(heap_.begin(), heap_.end());
            result.push_back(heap_.back());
            heap_.pop_back();
        }
        return result;
    }

public:
    basic_index_merger(const fs::path& target_dir,
        std::vector<fs::path> indices,
        int block_size,
        bool skip_unique = false)
        : target_dir_(target_dir),
          skip_unique_(skip_unique),
          block_size_(block_size)
    {
        for (fs::path index_dir : indices) {
            sources_.emplace_back(source_type::from(index_dir).value());
            indices_.emplace_back(&sources_.back());
        }
        terms_out_.open(index::terms_path(target_dir).c_str());
        doc_ids_.open(index::doc_ids_path(target_dir).c_str());
        doc_counts_.open(index::doc_counts_path(target_dir).c_str());
        doc_offset_ = 0;
        count_offset_ = 0;
    }

    int64_t copy_term(const entry& index_entry)
    {
        auto term_id = index_entry.current_term_id();
        doc_offset_ += index_entry.index()->copy_document_list(
            term_id, doc_ids_);
        count_offset_ += index_entry.index()->copy_frequency_list(
            term_id, doc_counts_);
        term_dfs_.push_back(index_entry.index()->term_collection_frequency(term_id));
        return index_entry.index()->term_occurrences(term_id);
    }

    int64_t merge_term(std::vector<entry>& indices)
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
        int64_t occurrences = 0;
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
        ir::Standard_Block_List_Builder<document_type, document_codec_type, true> doc_list_builder(
            block_size_);
        for (const auto& doc : doc_ids) { doc_list_builder.add(doc); }
        doc_offset_ += doc_list_builder.write(doc_ids_);

        ir::Standard_Block_List_Builder<frequency_type, frequency_codec_type, false>
            count_list_builder(block_size_);
        for (const auto& count : doc_counts) { count_list_builder.add(count); }
        count_offset_ += count_list_builder.write(doc_counts_);

        return occurrences;
    }


    int64_t merge_terms()
    {
        // Initialize heap: everyone starts with the first term.
        std::vector<std::unique_ptr<std::ifstream>> term_streams;
        document_type shift(0);
        int index_num = 0;
        for (const index_type& index : indices_) {
            term_streams.push_back(std::make_unique<std::ifstream>(
                (sources_[index_num].dir() / "terms.txt").c_str()));
            std::string current_term;
            *term_streams.back() >> current_term;
            heap_.emplace_back(index_num, 0, &index, shift, current_term);
            shift += static_cast<document_type>(index.collection_size());
            index_num++;
        }

        auto log = spdlog::get("buildindex");

        int64_t all_occurrences = 0;
        std::vector<int64_t> occurrences;
        term_id_type term_id = 0;
        while (not heap_.empty())
        {
            std::vector<entry> indices_to_merge = indices_with_next_term();
            if (log) {
                log->info(
                    "Merging term #{} from {} indices",
                    term_id++,
                    indices_to_merge.size());
            }
            occurrences.push_back(merge_term(indices_to_merge));
            all_occurrences += occurrences.back();
            for (entry& e : indices_to_merge) {
                if (e.current_term_id() + 1 < e.term_count())
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

        return all_occurrences;
    }

    void merge_titles()
    {
        std::ofstream tout(index::titles_path(target_dir_).c_str());
        for (const auto& index : indices_) {
            for (const std::string& title : index.titles()) {
                tout << title << std::endl;
            }
        }
    }

    std::tuple<int32_t, double, int32_t> merge_sizes()
    {
        std::ofstream sout(index::doc_sizes_path(target_dir_).c_str());
        return index::detail::merge_sizes(indices_, sout);
    }

    void write_properties(
        int32_t documents,
        int64_t occurrences,
        double avg_doc_size,
        int32_t max_doc_size)
    {
        index::Properties props;
        props.document_count = documents;
        props.occurrences_count = occurrences;
        props.skip_block_size = block_size_;
        props.avg_document_size = avg_doc_size;
        props.max_document_size = max_doc_size;
        index::Properties::write(props, target_dir_);
    }

    void merge()
    {
        auto log = spdlog::get("buildindex");
        if (log) { log->info("Merging titles"); }
        merge_titles();
        if (log) { log->info("Merging terms"); }
        int64_t occurrences = merge_terms();
        if (log) { log->info("Merging sizes"); }
        auto [documents, avg_doc_size, max_doc_size] = merge_sizes();
        if (log) { log->info("Writing properties"); }
        write_properties(documents, occurrences, avg_doc_size, max_doc_size);
    }
};

using index_merger = basic_index_merger<>;

}  // namespace irk
