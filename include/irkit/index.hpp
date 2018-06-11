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

//! \file index.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <algorithm>
#include <bitset>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <gsl/span>
#include <nlohmann/json.hpp>
#include <range/v3/utility/concepts.hpp>

#include <irkit/coding.hpp>
#include <irkit/coding/varbyte.hpp>
#include <irkit/compacttable.hpp>
#include <irkit/daat.hpp>
#include <irkit/index/list.hpp>
#include <irkit/index/posting_list.hpp>
#include <irkit/index/postingrange.hpp>
#include <irkit/io.hpp>
#include <irkit/io/memorybuffer.hpp>
#include <irkit/memoryview.hpp>
#include <irkit/prefixmap.hpp>
#include <irkit/score.hpp>
#include <irkit/types.hpp>

namespace irk::index {

    fs::path properties_path(fs::path dir) { return dir / "properties.json"; };
    fs::path doc_ids_path(fs::path dir) { return dir / "doc.id"; };
    fs::path doc_ids_off_path(fs::path dir) { return dir / "doc.idoff"; };
    fs::path doc_counts_path(fs::path dir) { return dir / "doc.count"; };
    fs::path doc_counts_off_path(fs::path dir) { return dir / "doc.countoff"; };
    fs::path terms_path(fs::path dir) { return dir / "terms.txt"; };
    fs::path term_map_path(fs::path dir) { return dir / "terms.map"; };
    fs::path term_doc_freq_path(fs::path dir) { return dir / "terms.docfreq"; };
    fs::path titles_path(fs::path dir) { return dir / "titles.txt"; };
    fs::path title_map_path(fs::path dir) { return dir / "titles.map"; };

};  // namespace index

namespace irk {

inline namespace v2 {

    class inverted_index_view;

    class inverted_index_mapped_data_source {
        using mapped_file_source = boost::iostreams::mapped_file_source;

    public:
        explicit inverted_index_mapped_data_source(fs::path dir)
        {
            io::enforce_exist(index::doc_ids_path(dir));
            io::enforce_exist(index::doc_counts_path(dir));
            io::enforce_exist(index::doc_ids_off_path(dir));
            io::enforce_exist(index::doc_counts_off_path(dir));
            io::enforce_exist(index::term_doc_freq_path(dir));
            io::enforce_exist(index::term_map_path(dir));
            io::enforce_exist(index::title_map_path(dir));

            documents_.open(index::doc_ids_path(dir));
            counts_.open(index::doc_counts_path(dir));
            document_offsets_.open(index::doc_ids_off_path(dir));
            count_offsets_.open(index::doc_counts_off_path(dir));
            term_collection_frequencies_.open(index::term_doc_freq_path(dir));
            term_map_.open(index::term_map_path(dir));
            title_map_.open(index::title_map_path(dir));
        }

        memory_view documents_view() const
        { return make_memory_view(documents_.data(), documents_.size()); }

        memory_view counts_view() const
        { return make_memory_view(counts_.data(), counts_.size()); }

        memory_view document_offsets_view() const
        {
            return make_memory_view(
                document_offsets_.data(), document_offsets_.size());
        }

        memory_view count_offsets_view() const
        {
            return make_memory_view(
                count_offsets_.data(), count_offsets_.size());
        }

        memory_view term_collection_frequencies_view() const
        {
            return make_memory_view(term_collection_frequencies_.data(),
                term_collection_frequencies_.size());
        }

        memory_view term_map_source() const
        { return make_memory_view(term_map_.data(), term_map_.size()); }

        memory_view title_map_source() const
        { return make_memory_view(title_map_.data(), title_map_.size()); }

    private:
        mapped_file_source documents_;
        mapped_file_source counts_;
        mapped_file_source document_offsets_;
        mapped_file_source count_offsets_;
        mapped_file_source term_collection_frequencies_;
        mapped_file_source term_map_;
        mapped_file_source title_map_;
    };

    class inverted_index_view {
    public:
        using document_type = long;
        using frequency_type = long;
        using offset_table_type =
            compact_table<long, irk::coding::varbyte_codec<long>, memory_view>;
        using frequency_table_type = compact_table<frequency_type,
            irk::coding::varbyte_codec<frequency_type>,
            memory_view>;
        using array_stream = boost::iostreams::stream_buffer<
            boost::iostreams::basic_array_source<char>>;

        inverted_index_view() = default;
        inverted_index_view(const inverted_index_view&) = default;
        inverted_index_view(inverted_index_view&&) = default;
        inverted_index_view(inverted_index_mapped_data_source data,
            any_codec<document_type> document_codec,
            any_codec<frequency_type> frequency_codec)
            : documents_view_(data.documents_view()),
              counts_view_(data.counts_view()),
              document_codec_(document_codec),
              frequency_codec_(frequency_codec),
              document_offsets_(data.document_offsets_view()),
              count_offsets_(data.count_offsets_view()),
              term_collection_frequencies_(
                  data.term_collection_frequencies_view()),
              term_map_(load_prefix_map<long>(data.term_map_source())),
              title_map_(load_prefix_map<long>(data.title_map_source())),
              term_count_(term_collection_frequencies_.size())
        {
            assert(document_offsets_.size() == term_count_);
            assert(count_offsets_.size() == term_count_);
        }
        inverted_index_view(memory_view documents_view,
            memory_view counts_view,
            memory_view document_offsets_view,
            memory_view count_offsets_view,
            memory_view term_frequencies_view,
            std::istream& term_map_stream,
            std::istream& title_map_stream,
            any_codec<document_type> document_codec,
            any_codec<frequency_type> frequency_codec)
            : documents_view_(std::move(documents_view)),
              counts_view_(std::move(counts_view)),
              document_codec_(document_codec),
              frequency_codec_(frequency_codec),
              document_offsets_(std::move(document_offsets_view)),
              count_offsets_(std::move(count_offsets_view)),
              term_collection_frequencies_(std::move(term_frequencies_view)),
              term_map_(load_prefix_map<long>(term_map_stream)),
              title_map_(load_prefix_map<long>(title_map_stream)),
              term_count_(term_collection_frequencies_.size())
        {
            assert(document_offsets_.size() == term_count_);
            assert(count_offsets_.size() == term_count_);
        }

        auto documents(long term_id) const
        {
            assert(term_id < term_count_);
            auto length = term_collection_frequencies_[term_id];
            auto document_offset = document_offsets_[term_id];
            return index::block_document_list_view(
                document_codec_, documents_view_, length, document_offset);
        }

        auto frequencies(long term_id) const
        {
            assert(term_id < term_count_);
            auto length = term_collection_frequencies_[term_id];
            auto count_offset = count_offsets_[term_id];
            return index::block_payload_list_view(
                frequency_codec_, counts_view_, length, count_offset);
        }

        auto postings(long term_id) const
        {
            assert(term_id < term_count_);
            auto length = term_collection_frequencies_[term_id];
            auto document_offset = document_offsets_[term_id];
            auto documents = index::block_document_list_view(
                document_codec_, documents_view_, length, document_offset);
            auto count_offset = count_offsets_[term_id];
            auto counts = index::block_payload_list_view(
                frequency_codec_, counts_view_, length, count_offset);
            return posting_list_view(documents, counts);
        }

    private:
        memory_view documents_view_;
        memory_view counts_view_;
        any_codec<document_type> document_codec_;
        any_codec<frequency_type> frequency_codec_;
        offset_table_type document_offsets_;
        offset_table_type count_offsets_;
        frequency_table_type term_collection_frequencies_;
        prefix_map<long, std::vector<char>> term_map_;
        prefix_map<long, std::vector<char>> title_map_;
        long term_count_;
    };

};  // namespace v2

namespace fs = boost::filesystem;

template<class T>
using varbyte_codec = coding::varbyte_codec<T>;

template<class Posting, class Freq, class Scorer>
using dspr = dynamically_scored_posting_range<Posting, Freq, Scorer>;

struct index_load_exception : public std::exception {
    fs::path file;
    std::ios_base::failure reason;

    index_load_exception(fs::path file, std::ios_base::failure reason)
        : file(file), reason(reason)
    {}
    const char* what() const throw()
    {
        std::string msg = "index_load_exception: Failed to read: "
            + file.string() + "; reason: " + reason.what();
        return msg.c_str();
    }
};

template<class Doc = std::size_t,
    class Term = std::string,
    class TermId = std::size_t,
    class Freq = std::size_t>
class inverted_index {
public:
    using document_type = Doc;
    using term_type = Term;
    using term_id_type = TermId;
    using frequency_type = Freq;

private:
    class TermHash {
    public:
        size_t operator()(const term_type* term) const
        {
            return std::hash<std::string>()(*term);
        }
    };

    class TermEqual {
    public:
        bool operator()(const term_type* lhs, const term_type* rhs) const
        {
            return *lhs == *rhs;
        }
    };

    const fs::path dir_;
    bool in_memory_;
    bool skip_term_map_;
    std::vector<std::shared_ptr<term_type>> terms_;
    mapped_compact_table<Freq> term_dfs_;
    std::vector<char> doc_ids_;
    mapped_offset_table<> doc_ids_off_;
    std::vector<char> doc_counts_;
    mapped_offset_table<> doc_counts_off_;
    std::vector<std::string> titles_;
    std::size_t doc_ids_size_;
    std::size_t doc_counts_size_;
    std::unordered_map<const term_type*, term_id_type, TermHash, TermEqual>
        term_map_;
    nlohmann::json properties_;

    std::optional<std::size_t> offset(const std::string& term,
        const mapped_offset_table<>& offset_table) const
    {
        auto pos = term_map_.find(&term);
        if (pos == term_map_.end()) {
            return std::nullopt;
        }
        term_id_type term_id = pos->second;
        return offset(term_id, offset_table);
    }

    std::pair<std::size_t, std::size_t> locate(term_id_type term_id,
        const mapped_offset_table<>& offset_table,
        std::size_t file_size) const
    {
        std::size_t offset = offset_table[term_id];
        std::size_t following = std::size_t(term_id) + 1 < offset_table.size()
            ? offset_table[term_id + 1]
            : file_size;
        return std::make_pair(offset, following - offset);
    }

    template<class T>
    std::vector<T> decode_range(term_id_type term_id,
        const std::vector<char>& data_container,
        const mapped_offset_table<>& offset_table,
        bool delta) const
    {
        if (term_id >= offset_table.size()) {
            throw std::out_of_range(
                "Requested term ID out of range; requested: "
                + std::to_string(term_id) + " but must be [0, "
                + std::to_string(term_map_.size()) + ")");
        }
        auto[offset, range_size] =
            locate(term_id, offset_table, data_container.size());
        return delta
            ? irk::coding::decode_delta<varbyte_codec<T>>(gsl::span<const char>(
                  data_container.data() + offset, range_size))
            : irk::coding::decode<varbyte_codec<T>>(gsl::span<const char>(
                  data_container.data() + offset, range_size));
    }

    std::size_t offset(
        term_id_type term_id, const mapped_offset_table<>& offset_table) const
    {
        if (term_id >= term_map_.size()) {
            throw std::out_of_range(
                "Requested term ID out of range; requested: "
                + std::to_string(term_id) + " but must be [0, "
                + std::to_string(term_map_.size()) + ")");
        }
        return offset_table[term_id];
    }

    void enforce_exist(fs::path file) const
    {
        if (!fs::exists(file)) {
            throw std::invalid_argument(
                "File not found: " + file.generic_string());
        }
    }

public:
    inverted_index(std::vector<term_type> terms,
        mapped_compact_table<Freq> term_dfs,
        std::vector<char> doc_ids,
        mapped_offset_table<> doc_ids_off,
        std::vector<char> doc_counts,
        mapped_offset_table<> doc_counts_off,
        std::vector<std::string> titles)
        : dir_(""),
          in_memory_(true),
          skip_term_map_(false),
          term_dfs_(std::move(term_dfs)),
          doc_ids_(std::move(doc_ids)),
          doc_ids_off_(std::move(doc_ids_off)),
          doc_counts_(std::move(doc_counts)),
          doc_counts_off_(std::move(doc_counts_off)),
          titles_(std::move(titles)),
          doc_ids_size_(doc_ids_.size()),
          doc_counts_size_(doc_counts_.size())
    {
        term_id_type term_id(0);
        for (const term_type& term : terms) {
            terms_.push_back(std::make_shared<std::string>(std::move(term)));
            term_map_[terms_.back().get()] = term_id++;
        }
    }
    inverted_index(fs::path dir,
        bool in_memory = true,
        bool skip_term_map = false,
        bool verbose = false)
        : dir_(dir),
          in_memory_(in_memory),
          skip_term_map_(skip_term_map),
          term_dfs_(map_compact_table<Freq>(index::term_doc_freq_path(dir))),
          doc_ids_off_(map_offset_table<>(index::doc_ids_off_path(dir))),
          doc_counts_off_(map_offset_table<>(index::doc_counts_off_path(dir)))
    {
        using namespace std::chrono;

        load_properties(index::properties_path(dir));

        std::cout << "Loading term map... " << std::flush;
        auto start_time = steady_clock::now();
        load_term_map(index::terms_path(dir));
        auto elapsed =
            duration_cast<milliseconds>(steady_clock::now() - start_time);
        std::cout << elapsed.count() << " ms" << std::endl;

        if (in_memory) {
            std::cout << "Loading postings... " << std::flush;
            start_time = steady_clock::now();
            load_data(index::doc_ids_path(dir), doc_ids_);
            load_data(index::doc_counts_path(dir), doc_counts_);
            elapsed =
                duration_cast<milliseconds>(steady_clock::now() - start_time);
            std::cout << elapsed.count() << " ms" << std::endl;
        }

        std::cout << "Loading titles... " << std::flush;
        start_time = steady_clock::now();
        load_titles(index::titles_path(dir));
        elapsed = duration_cast<milliseconds>(steady_clock::now() - start_time);
        std::cout << elapsed.count() << " ms" << std::endl;

        doc_ids_size_ = file_size(index::doc_ids_path(dir));
        doc_counts_size_ = file_size(index::doc_counts_path(dir));
    }

    void load_properties(fs::path properties_file)
    {
        if (fs::exists(properties_file)) {
            std::ifstream is(properties_file.c_str());
            is >> properties_;
            is.close();
        }
    }

    //void load_term_dfs(fs::path term_df_file)
    //{
    //    enforce_exist(term_df_file);
    //    std::vector<char> data;
    //    load_data(term_df_file, data);
    //    term_dfs_ = irk::coding::decode<varbyte_codec<Freq>>(data);
    //}
    void load_titles(fs::path titles_file)
    {
        enforce_exist(titles_file);
        std::ifstream ifs(titles_file.c_str());
        std::string title;
        try {
            while (std::getline(ifs, title)) {
                titles_.push_back(title);
            }
        } catch (std::ios_base::failure& fail) {
            throw index_load_exception(titles_file, fail);
        }
        ifs.close();
    }
    void load_term_map(fs::path term_file)
    {
        enforce_exist(term_file);
        std::ifstream ifs(term_file.c_str());
        std::string term;
        term_id_type term_id(0);
        while (std::getline(ifs, term)) {
            terms_.push_back(std::make_shared<std::string>(std::move(term)));
            if (!skip_term_map_) {
                term_map_[terms_.back().get()] = term_id++;
            }
        }
        ifs.close();
    }

    void load_data(fs::path data_file, std::vector<char>& data_container) const
    {
        enforce_exist(data_file);
        std::ifstream ifs(data_file.c_str(), std::ios::binary);
        ifs.seekg(0, std::ios::end);
        std::streamsize size = ifs.tellg();
        ifs.seekg(0, std::ios::beg);
        data_container.resize(size);
        if (!ifs.read(data_container.data(), size)) {
            throw std::runtime_error("Failed reading " + data_file.string());
        }
        ifs.close();
    }

    std::size_t file_size(fs::path file)
    {
        enforce_exist(file);
        std::ifstream ifs(file.c_str(), std::ios::binary);
        ifs.seekg(0, std::ios::end);
        std::streamsize size = ifs.tellg();
        ifs.close();
        return size;
    }

    std::vector<char>
    load_data(fs::path data_file, std::size_t start, std::size_t size) const
    {
        enforce_exist(data_file);
        std::ifstream ifs(data_file.c_str(), std::ios::binary);
        ifs.seekg(start, std::ios::beg);
        std::vector<char> data_container;
        data_container.resize(size);
        if (!ifs.read(data_container.data(), size)) {
            throw std::runtime_error("Failed reading " + data_file.string());
        }
        ifs.close();
        return data_container;
    }

    std::size_t collection_size() const { return titles_.size(); }

    template<class ScoreFn = score::tf_idf_scorer>
    auto empty_posting_range() const
        -> dspr<_posting<document_type,
                    score::score_result_t<ScoreFn, document_type, Freq>>,
            Freq,
            ScoreFn>
    {
        using Posting = _posting<document_type,
            score::score_result_t<ScoreFn, document_type, Freq>>;
        return dspr<Posting, Freq, ScoreFn>(std::vector<document_type>(),
            std::vector<Freq>(),
            Freq(0),
            0,
            ScoreFn{});
    }

    template<class ScoreFn = score::tf_idf_scorer>
    auto posting_ranges(const std::vector<std::string>& terms,
        ScoreFn score_fn = score::tf_idf_scorer{}) const
        -> std::vector<
            dspr<_posting<document_type, score::score_result_t<ScoreFn, document_type, Freq>>,
                Freq,
                ScoreFn>>
    {
        using Posting = _posting<document_type, score::score_result_t<ScoreFn, document_type, Freq>>;
        std::vector<dspr<Posting, Freq, ScoreFn>> ranges;
        for (const auto& term : terms) {
            ranges.push_back(posting_range(term, score_fn));
        }
        return ranges;
    }

    template<class ScoreFn = score::tf_idf_scorer>
    auto posting_range(const std::string& term,
        ScoreFn score_fn = score::tf_idf_scorer{}) const
        -> dspr<_posting<document_type, score::score_result_t<ScoreFn, document_type, Freq>>,
            Freq,
            ScoreFn>
    {
        auto opt = term_id(term);
        if (opt == std::nullopt) {
            return empty_posting_range<ScoreFn>();
        }
        return posting_range(opt.value(), score_fn);
    }

    template<class ScoreFn = score::tf_idf_scorer>
    auto posting_range(
        term_id_type term_id, ScoreFn score_fn = score::tf_idf_scorer{}) const
        -> dspr<_posting<document_type,
                    score::score_result_t<ScoreFn, document_type, Freq>>,
            Freq,
            ScoreFn>
    {
        using Posting = _posting<document_type,
            score::score_result_t<ScoreFn, document_type, Freq>>;
        Freq df = term_dfs_[term_id];
        std::vector<document_type> docs;
        std::vector<Freq> tfs;
        if (in_memory_) {
            docs = decode_range<document_type>(
                term_id, doc_ids_, doc_ids_off_, true);
            tfs = decode_range<Freq>(
                term_id, doc_counts_, doc_counts_off_, false);
        } else {
            auto[doc_offset, doc_range_size] =
                locate(term_id, doc_ids_off_, doc_ids_size_);
            auto[count_offset, count_range_size] =
                locate(term_id, doc_counts_off_, doc_counts_size_);
            docs = irk::coding::decode_delta<varbyte_codec<document_type>>(
                load_data(
                    index::doc_ids_path(dir_), doc_offset, doc_range_size));
            tfs = irk::coding::decode<varbyte_codec<Freq>>(load_data(
                index::doc_counts_path(dir_), count_offset, count_range_size));
        }
        return dspr<Posting, Freq, ScoreFn>(
            std::move(docs), std::move(tfs), df, titles_.size(), score_fn);
    }

    std::string title(document_type doc_id) const { return titles_[doc_id]; }
    const std::vector<std::string>& titles() const { return titles_; }
    term_type term(term_id_type term_id) const { return *terms_[term_id]; }
    std::optional<term_id_type> term_id(term_type term) const
    {
        auto it = term_map_.find(&term);
        if (it == term_map_.end()) {
            return std::nullopt;
        }
        return std::make_optional(it->second);
    }
    std::size_t term_count() const { return terms_.size(); }
};

using default_index =
    inverted_index<std::uint32_t, std::string, std::uint32_t, std::uint32_t>;

};  // namespace irk
