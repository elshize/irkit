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
#include <boost/log/trivial.hpp>
#include <gsl/span>
#include <nlohmann/json.hpp>
#include <range/v3/utility/concepts.hpp>
#include <type_safe/strong_typedef.hpp>
#include <type_safe/config.hpp>
#include <type_safe/types.hpp>
#include <type_safe/index.hpp>

#include <irkit/assert.hpp>
#include <irkit/coding.hpp>
#include <irkit/coding/varbyte.hpp>
#include <irkit/compacttable.hpp>
#include <irkit/daat.hpp>
#include <irkit/index/block_inverted_list.hpp>
#include <irkit/index/posting_list.hpp>
#include <irkit/index/postingrange.hpp>
#include <irkit/index/types.hpp>
#include <irkit/io.hpp>
#include <irkit/lexicon.hpp>
#include <irkit/memoryview.hpp>
#include <irkit/score.hpp>
#include <irkit/types.hpp>

namespace ts = type_safe;

namespace irk {

namespace index {

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
    fs::path doc_sizes_path(fs::path dir) { return dir / "doc.sizes"; };
    fs::path term_occurrences_path(fs::path dir)
    {
        return dir / "term.occurrences";
    };

};  // namespace index

class inverted_index_view {
public:
    using document_type = long;
    using frequency_type = long;
    using size_type = long;
    using score_type = long;
    using offset_table_type = compact_table<index::offset_t,
        irk::varbyte_codec<index::offset_t>,
        memory_view>;
    using frequency_table_type = compact_table<frequency_type,
        irk::varbyte_codec<frequency_type>,
        memory_view>;
    using size_table_type =
        compact_table<long, irk::varbyte_codec<long>, memory_view>;
    using array_stream = boost::iostreams::stream_buffer<
        boost::iostreams::basic_array_source<char>>;

    inverted_index_view() = default;
    inverted_index_view(const inverted_index_view&) = default;
    inverted_index_view(inverted_index_view&&) = default;

    template<class DataSourceT>
    inverted_index_view(DataSourceT* data,
        any_codec<document_type> document_codec,
        any_codec<frequency_type> frequency_codec,
        any_codec<score_type> score_codec)
        : documents_view_(data->documents_view()),
          counts_view_(data->counts_view()),
          scores_view_(data->scores_source()),
          document_codec_(document_codec),
          frequency_codec_(frequency_codec),
          score_codec_(score_codec),
          document_offsets_(data->document_offsets_view()),
          count_offsets_(data->count_offsets_view()),
          document_sizes_(data->document_sizes_view()),
          score_offsets_(std::nullopt),
          term_collection_frequencies_(
              data->term_collection_frequencies_view()),
          term_collection_occurrences_(
              data->term_collection_occurrences_view()),
          term_map_(std::move(load_lexicon(data->term_map_source()))),
          title_map_(std::move(load_lexicon(data->title_map_source()))),
          term_count_(term_collection_frequencies_.size())
    {
        EXPECTS(document_offsets_.size() == term_count_);
        EXPECTS(count_offsets_.size() == term_count_);

        if (data->score_offset_source().has_value())
        {
            score_offsets_ = std::make_optional<offset_table_type>(
                data->score_offset_source().value());
        }
        std::string buffer(
            data->properties_view().data(), data->properties_view().size());
        nlohmann::json properties = nlohmann::json::parse(buffer);
        document_count_ = properties["documents"];
        occurrences_count_ = properties["occurrences"];
        block_size_ = properties["skip_block_size"];
        avg_document_size_ = properties["avg_document_size"];
    }

    size_type collection_size() const { return document_sizes_.size(); }

    size_type document_size(document_type doc) const
    {
        return document_sizes_[doc];
    }

    auto documents(long term_id) const
    {
        EXPECTS(term_id < term_count_);
        auto length = term_collection_frequencies_[term_id];
        return index::block_document_list_view(document_codec_,
            select(term_id, document_offsets_, documents_view_),
            length);
    }

    auto frequencies(long term_id) const
    {
        EXPECTS(term_id < term_count_);
        auto length = term_collection_frequencies_[term_id];
        return index::block_payload_list_view(frequency_codec_,
            select(term_id, count_offsets_, counts_view_),
            length);
    }

    auto scores(long term_id) const
    {
        EXPECTS(term_id < term_count_);
        auto length = term_collection_frequencies_[term_id];
        return index::block_payload_list_view(frequency_codec_,
            select(term_id, *score_offsets_, *scores_view_),
            length);
    }

    auto postings(long term_id) const
    {
        EXPECTS(term_id < term_count_);
        auto length = term_collection_frequencies_[term_id];
        auto documents = index::block_document_list_view(document_codec_,
            select(term_id, document_offsets_, documents_view_),
            length);
        auto counts = index::block_payload_list_view(frequency_codec_,
            select(term_id, count_offsets_, counts_view_),
            length);
        return posting_list_view(documents, counts);
    }

    auto postings(const std::string& term) const
    {
        auto idopt = term_id(term);
        if (!idopt.has_value())
        { throw std::runtime_error("TODO: implement empty posting list"); }
        return postings(*idopt);
    }

    auto scored_postings(long term_id) const
    {
        EXPECTS(term_id < term_count_);
        if (!scores_view_.has_value())
        { throw std::runtime_error("scores not loaded"); }
        auto length = term_collection_frequencies_[term_id];
        auto documents = index::block_document_list_view(document_codec_,
            select(term_id, document_offsets_, documents_view_),
            length);
        auto scores = index::block_payload_list_view(score_codec_,
            select(term_id, *score_offsets_, *scores_view_),
            length);
        return posting_list_view(documents, scores);
    }

    template<class Scorer>
    Scorer term_scorer(long term_id) const
    {
        if constexpr (std::is_same<Scorer, score::bm25_scorer>::value)
        {
            return score::bm25_scorer(term_collection_frequencies_[term_id],
                document_count_,
                avg_document_size_);
        } else if constexpr (std::is_same<Scorer,
                                 score::query_likelihood_scorer>::value)
        {
            return score::query_likelihood_scorer(
                term_occurrences(term_id), occurrences_count());
        }
    }

    std::optional<long> term_id(const std::string& term) const
    {
        return term_map_.index_at(term);
    }

    std::string term(const long& id) const { return term_map_.key_at(id); }

    long tdf(long term_id) const
    {
        return term_collection_frequencies_[term_id];
    }

    long term_occurrences(long term_id) const
    {
        return term_collection_occurrences_[term_id];
    }

    long term_count() const { return term_map_.size(); }
    long occurrences_count() const { return occurrences_count_; }
    int skip_block_size() const { return block_size_; }
    int avg_document_size() const { return avg_document_size_; }

    const auto& terms() const
    {
        return term_map_;
    }
    const auto& titles() const
    {
        return title_map_;
    }
    any_codec<document_type> document_codec() { return document_codec_; }
    any_codec<frequency_type> frequency_codec() { return frequency_codec_; }

    std::streamsize copy_document_list(long term_id, std::ostream& out) const
    {
        auto offset = document_offsets_[term_id];
        return copy_list(documents_view_, offset, out);
    }

    std::streamsize copy_frequency_list(long term_id, std::ostream& out) const
    {
        auto offset = count_offsets_[term_id];
        return copy_list(counts_view_, offset, out);
    }

private:
    memory_view documents_view_;
    memory_view counts_view_;
    std::optional<memory_view> scores_view_;
    any_codec<document_type> document_codec_;
    any_codec<frequency_type> frequency_codec_;
    any_codec<frequency_type> score_codec_;
    offset_table_type document_offsets_;
    offset_table_type count_offsets_;
    size_table_type document_sizes_;
    std::optional<offset_table_type> score_offsets_;
    frequency_table_type term_collection_frequencies_;
    frequency_table_type term_collection_occurrences_;
    lexicon<hutucker_codec<char>, memory_view> term_map_;
    lexicon<hutucker_codec<char>, memory_view> title_map_;
    std::ptrdiff_t term_count_;
    std::ptrdiff_t document_count_;
    std::ptrdiff_t occurrences_count_;
    int block_size_;
    double avg_document_size_;

    std::streamsize copy_list(const memory_view& memory,
        std::streamsize offset,
        std::ostream& sink) const
    {
        irk::varbyte_codec<long> vb;
        const char* list_ptr = memory.data() + offset;
        boost::iostreams::stream_buffer<
            boost::iostreams::basic_array_source<char>>
            buf(list_ptr, list_ptr + sizeof(long) + 1);
        std::istream istr(&buf);
        long size;
        vb.decode(istr, size);
        sink.write(list_ptr, size);
        return size;
    }

    memory_view select(long term_id,
        const offset_table_type& offsets,
        const memory_view& memory) const
    {
        index::offset_t offset = offsets[term_id];
        index::offset_t next_offset = (term_id + 1 < term_count_)
            ? offsets[term_id + 1]
            : memory.size();
        std::cout << "[" << offset << " " << next_offset - 1
                  << "] term: " << term_id << "/" << term_count_ << std::endl;
        return memory[{offset, next_offset - 1}];
    }
    };

    template<class Scorer, class DataSourceT>
    void score_index(fs::path dir_path, int bits)
    {
        std::string name(typename Scorer::tag_type{});
        fs::path scores_path = dir_path / (name + ".scores");
        fs::path score_offsets_path = dir_path / (name + ".offsets");
        DataSourceT source(dir_path);
        inverted_index_view index(&source,
            irk::varbyte_codec<long>{},
            irk::varbyte_codec<long>{},
            irk::varbyte_codec<long>{});

        long collection_size = index.collection_size();
        long offset = 0;
        std::vector<std::size_t> offsets;
        std::ofstream sout(scores_path.c_str());
        std::ofstream offout(score_offsets_path.c_str());

        BOOST_LOG_TRIVIAL(info) << "Calculating max score." << std::flush;
        double max_score = 0;
        for (long term_id = 0; term_id < index.terms().size(); term_id++)
        {
            Scorer scorer = index.term_scorer<Scorer>(term_id);
            for (const auto& posting : index.postings(term_id))
            {
                double score = scorer(
                    posting.payload(), index.document_size(posting.document()));
                max_score = std::max(max_score, score);
            }
        }
        BOOST_LOG_TRIVIAL(info) << "Max score: " << max_score << std::flush;

        BOOST_LOG_TRIVIAL(info) << "Scoring..." << std::flush;
        long max_int = (1 << bits) - 1;
        for (long term_id = 0; term_id < index.terms().size(); term_id++)
        {
            offsets.push_back(offset);
            irk::index::block_list_builder<long, false> list_builder(
                index.skip_block_size(), irk::varbyte_codec<long>{});
            Scorer scorer = index.term_scorer<Scorer>(term_id);
            for (const auto& posting : index.postings(term_id))
            {
                double score = scorer(
                    posting.payload(), index.document_size(posting.document()));
                long quantized_score = (long)(score * max_int / max_score);
                list_builder.add(quantized_score);
            }
            offset += list_builder.write(sout);
        }
        irk::offset_table<> offset_table = irk::build_offset_table<>(offsets);
        offout << offset_table;
    }

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
            ? irk::decode_delta<varbyte_codec<T>>(gsl::span<const char>(
                  data_container.data() + offset, range_size))
            : irk::decode<varbyte_codec<T>>(gsl::span<const char>(
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
    //    term_dfs_ = irk::decode<varbyte_codec<Freq>>(data);
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
            docs = irk::decode_delta<varbyte_codec<document_type>>(
                load_data(
                    index::doc_ids_path(dir_), doc_offset, doc_range_size));
            tfs = irk::decode<varbyte_codec<Freq>>(load_data(
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
