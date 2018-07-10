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
#include <type_safe/config.hpp>
#include <type_safe/index.hpp>
#include <type_safe/strong_typedef.hpp>
#include <type_safe/types.hpp>

#include <irkit/assert.hpp>
#include <irkit/coding.hpp>
#include <irkit/coding/stream_vbyte.hpp>
#include <irkit/compacttable.hpp>
#include <irkit/daat.hpp>
#include <irkit/index/block_inverted_list.hpp>
#include <irkit/index/posting_list.hpp>
#include <irkit/index/types.hpp>
#include <irkit/io.hpp>
#include <irkit/lexicon.hpp>
#include <irkit/memoryview.hpp>
#include <irkit/score.hpp>
#include <irkit/types.hpp>

namespace ts = type_safe;

namespace irk {

using index::term_id_t;
using index::offset_t;

namespace index {

    inline fs::path properties_path(const fs::path& dir)
    { return dir / "properties.json"; };
    inline fs::path doc_ids_path(const fs::path& dir)
    { return dir / "doc.id"; };
    inline fs::path doc_ids_off_path(const fs::path& dir)
    { return dir / "doc.idoff"; };
    inline fs::path doc_counts_path(const fs::path& dir)
    { return dir / "doc.count"; };
    inline fs::path doc_counts_off_path(const fs::path& dir)
    { return dir / "doc.countoff"; };
    inline fs::path terms_path(const fs::path& dir)
    { return dir / "terms.txt"; };
    inline fs::path term_map_path(const fs::path& dir)
    { return dir / "terms.map"; };
    inline fs::path term_doc_freq_path(const fs::path& dir)
    { return dir / "terms.docfreq"; };
    inline fs::path titles_path(const fs::path& dir)
    { return dir / "titles.txt"; };
    inline fs::path title_map_path(const fs::path& dir)
    { return dir / "titles.map"; };
    inline fs::path doc_sizes_path(const fs::path& dir)
    { return dir / "doc.sizes"; };
    inline fs::path term_occurrences_path(const fs::path& dir)
    { return dir / "term.occurrences"; };

};  // namespace index

template<class DocumentCodec = irk::stream_vbyte_codec<index::document_t>,
    class FrequencyCodec = irk::stream_vbyte_codec<index::frequency_t>,
    class ScoreCodec = irk::stream_vbyte_codec<std::uint32_t>>
class basic_inverted_index_view {
public:
    using document_type = index::document_t;
    using document_codec_type = DocumentCodec;
    using frequency_codec_type = FrequencyCodec;
    using score_codec_type = ScoreCodec;
    using frequency_type = index::frequency_t;
    using size_type = int32_t;
    using score_type = std::uint32_t;
    using term_id_type = index::term_id_t;
    using offset_table_type = compact_table<index::offset_t,
        irk::vbyte_codec<index::offset_t>,
        memory_view>;
    using frequency_table_type = compact_table<frequency_type,
        irk::vbyte_codec<frequency_type>,
        memory_view>;
    using size_table_type =
        compact_table<int32_t, irk::vbyte_codec<int32_t>, memory_view>;
    using array_stream = boost::iostreams::stream_buffer<
        boost::iostreams::basic_array_source<char>>;

    basic_inverted_index_view() = delete;
    basic_inverted_index_view(const basic_inverted_index_view&) = default;
    basic_inverted_index_view(basic_inverted_index_view&&) noexcept = default;
    basic_inverted_index_view&
    operator=(const basic_inverted_index_view&) = default;
    basic_inverted_index_view&
    operator=(basic_inverted_index_view&&) noexcept = default;
    ~basic_inverted_index_view() = default;

    template<class DataSourceT>
    explicit basic_inverted_index_view(DataSourceT* data)
        : documents_view_(data->documents_view()),
          counts_view_(data->counts_view()),
          scores_view_(data->scores_source()),
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

    auto documents(term_id_type term_id) const
    {
        EXPECTS(term_id < term_count_);
        auto length = term_collection_frequencies_[term_id];
        return index::block_document_list_view<document_codec_type>(
            select(term_id, document_offsets_, documents_view_), length);
    }

    auto frequencies(term_id_type term_id) const
    {
        EXPECTS(term_id < term_count_);
        auto length = term_collection_frequencies_[term_id];
        return index::block_payload_list_view<frequency_type,
            frequency_codec_type>(
            select(term_id, count_offsets_, counts_view_), length);
    }

    auto scores(term_id_type term_id) const
    {
        EXPECTS(term_id < term_count_);
        auto length = term_collection_frequencies_[term_id];
        return index::block_payload_list_view<score_type, score_codec_type>(
            select(term_id, *score_offsets_, *scores_view_), length);
    }

    auto postings(term_id_type term_id) const
    {
        EXPECTS(term_id < term_count_);
        auto length = term_collection_frequencies_[term_id];
        auto documents = index::block_document_list_view<document_codec_type>(
            select(term_id, document_offsets_, documents_view_), length);
        auto counts = index::block_payload_list_view<frequency_type,
            frequency_codec_type>(
            select(term_id, count_offsets_, counts_view_), length);
        return posting_list_view(documents, counts);
    }

    auto postings(const std::string& term) const
    {
        auto idopt = term_id(term);
        if (not idopt.has_value())
        { throw std::runtime_error("TODO: implement empty posting list"); }
        return postings(*idopt);
    }

    auto scored_postings(term_id_type term_id) const
    {
        EXPECTS(term_id < term_count_);
        if (not scores_view_.has_value())
        { throw std::runtime_error("scores not loaded"); }
        auto length = term_collection_frequencies_[term_id];
        auto documents = index::block_document_list_view<document_codec_type>(
            select(term_id, document_offsets_, documents_view_), length);
        auto scores =
            index::block_payload_list_view<score_type, score_codec_type>(
                select(term_id, *score_offsets_, *scores_view_), length);
        return posting_list_view(documents, scores);
    }

    template<class Scorer>
    Scorer term_scorer(term_id_type term_id) const
    {
        if constexpr (std::is_same<Scorer,
                          score::bm25_scorer>::value) {  // NOLINT
            return score::bm25_scorer(term_collection_frequencies_[term_id],
                document_count_,
                avg_document_size_);
        }
        else if constexpr (std::is_same<Scorer,  // NOLINT
                                 score::query_likelihood_scorer>::value)
        {
            return score::query_likelihood_scorer(
                term_occurrences(term_id), occurrences_count());
        }
    }

    std::optional<term_id_type> term_id(const std::string& term) const
    {
        return term_map_.index_at(term);
    }

    std::string term(const term_id_type& id) const
    {
        return term_map_.key_at(id);
    }

    int32_t tdf(term_id_type term_id) const
    {
        return term_collection_frequencies_[term_id];
    }

    int64_t term_occurrences(term_id_type term_id) const
    {
        return term_collection_occurrences_[term_id];
    }

    int32_t term_count() const { return term_map_.size(); }
    int64_t occurrences_count() const { return occurrences_count_; }
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
    document_codec_type document_codec() { return document_codec_; }
    frequency_codec_type frequency_codec() { return frequency_codec_; }

    std::streamsize
    copy_document_list(term_id_type term_id, std::ostream& out) const
    {
        auto offset = document_offsets_[term_id];
        return copy_list(documents_view_, offset, out);
    }

    std::streamsize
    copy_frequency_list(term_id_type term_id, std::ostream& out) const
    {
        auto offset = count_offsets_[term_id];
        return copy_list(counts_view_, offset, out);
    }

private:
    memory_view documents_view_;
    memory_view counts_view_;
    std::optional<memory_view> scores_view_;
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

    static const constexpr auto document_codec_ = document_codec_type{};
    static const constexpr auto frequency_codec_ = frequency_codec_type{};
    static const constexpr auto score_codec_ = score_codec_type{};

    std::streamsize copy_list(const memory_view& memory,
        std::streamsize offset,
        std::ostream& sink) const
    {
        irk::vbyte_codec<offset_t> vb;
        const char* list_ptr = memory.data() + offset;
        offset_t size;
        vb.decode(list_ptr, &size);
        sink.write(list_ptr, size);
        return size;
    }

    memory_view select(term_id_type term_id,
        const offset_table_type& offsets,
        const memory_view& memory) const
    {
        offset_t offset = offsets[term_id];
        offset_t next_offset = (term_id + 1 < term_count_)
            ? offsets[term_id + 1]
            : memory.size();
        return memory(offset, next_offset);
    }
};

using inverted_index_view = basic_inverted_index_view<>;

template<class Scorer, class DataSourceT>
void score_index(fs::path dir_path, unsigned int bits)
{
    std::string name(typename Scorer::tag_type{});
    fs::path scores_path = dir_path / (name + ".scores");
    fs::path score_offsets_path = dir_path / (name + ".offsets");
    DataSourceT source(dir_path);
    inverted_index_view index(&source);

    int64_t offset = 0;
    std::vector<std::size_t> offsets;
    std::ofstream sout(scores_path.c_str());
    std::ofstream offout(score_offsets_path.c_str());

    BOOST_LOG_TRIVIAL(info) << "Calculating max score." << std::flush;
    double max_score = 0;
    for (term_id_t term_id = 0; term_id < index.terms().size(); term_id++)
    {
        auto scorer = index.term_scorer<Scorer>(term_id);
        for (const auto& posting : index.postings(term_id))
        {
            double score = scorer(
                posting.payload(), index.document_size(posting.document()));
            max_score = std::max(max_score, score);
        }
    }
    BOOST_LOG_TRIVIAL(info) << "Max score: " << max_score << std::flush;

    BOOST_LOG_TRIVIAL(info) << "Scoring..." << std::flush;
    int64_t max_int = (1u << bits) - 1u;
    for (term_id_t term_id = 0; term_id < index.terms().size(); term_id++)
    {
        offsets.push_back(offset);
        irk::index::block_list_builder<std::uint32_t,
            stream_vbyte_codec<std::uint32_t>,
            false>
            list_builder(index.skip_block_size());
        auto scorer = index.term_scorer<Scorer>(term_id);
        for (const auto& posting : index.postings(term_id))
        {
            double score = scorer(
                posting.payload(), index.document_size(posting.document()));
            auto quantized_score = static_cast<int64_t>(
                score * max_int / max_score);
            list_builder.add(quantized_score);
        }
        offset += list_builder.write(sout);
    }
    irk::offset_table<> offset_table = irk::build_offset_table<>(offsets);
    offout << offset_table;
};

};  // namespace irk
