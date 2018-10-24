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

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/range/adaptors.hpp>
#include <cppitertools/itertools.hpp>
#include <fmt/format.h>
#include <gsl/span>
#include <nlohmann/json.hpp>
#include <range/v3/utility/concepts.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <taily.hpp>
#include <type_safe/config.hpp>
#include <type_safe/index.hpp>
#include <type_safe/strong_typedef.hpp>
#include <type_safe/types.hpp>

#include <irkit/algorithm.hpp>
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
#include <irkit/quantize.hpp>
#include <irkit/score.hpp>
#include <irkit/types.hpp>

namespace ts = type_safe;

namespace irk {

using index::frequency_t;
using index::offset_t;
using index::term_id_t;

template<
    typename P,
    typename O = P,
    typename M = O,
    typename E = M,
    typename V = E>
struct score_tuple {
    P postings;
    O offsets;
    M max_scores;
    E exp_values;
    V variances;
};

namespace index {

    using boost::adaptors::filtered;
    using boost::filesystem::directory_iterator;
    using boost::filesystem::is_regular_file;
    using boost::filesystem::path;

    struct posting_paths {
        path postings;
        path offsets;
        path max_scores;
    };

    inline path properties_path(const path& dir)
    { return dir / "properties.json"; }
    inline path doc_ids_path(const path& dir)
    { return dir / "doc.id"; }
    inline path doc_ids_off_path(const path& dir)
    { return dir / "doc.idoff"; }
    inline path doc_counts_path(const path& dir)
    { return dir / "doc.count"; }
    inline path doc_counts_off_path(const path& dir)
    { return dir / "doc.countoff"; }
    inline path terms_path(const path& dir)
    { return dir / "terms.txt"; }
    inline path term_map_path(const path& dir)
    { return dir / "terms.map"; }
    inline path term_doc_freq_path(const path& dir)
    { return dir / "terms.docfreq"; }
    inline path titles_path(const path& dir)
    { return dir / "titles.txt"; }
    inline path title_map_path(const path& dir)
    { return dir / "titles.map"; }
    inline path doc_sizes_path(const path& dir)
    { return dir / "doc.sizes"; }
    inline path term_occurrences_path(const path& dir)
    { return dir / "term.occurrences"; }
    inline path score_offset_path(const path& dir, const std::string& name)
    { return dir / fmt::format("{}.offsets", name); }
    inline path max_scores_path(const path& dir, const std::string& name)
    { return dir / fmt::format("{}.maxscore", name); }

    inline score_tuple<path>
    score_paths(const path& dir, const std::string& name)
    {
        return {dir / fmt::format("{}.scores", name),
                dir / fmt::format("{}.offsets", name),
                dir / fmt::format("{}.maxscore", name),
                dir / fmt::format("{}.expscore", name),
                dir / fmt::format("{}.varscore", name)};
    }

    inline std::vector<std::string> all_score_names(const path& dir)
    {
        std::vector<std::string> names;
        auto matches = [&](const path& p) {
            return boost::algorithm::ends_with(
                p.filename().string(), ".scores");
        };
        for (auto& file :
             boost::make_iterator_range(directory_iterator(dir), {}))
        {
            if (is_regular_file(file.path()) && matches(file.path())) {
                std::string filename = file.path().filename().string();
                std::string name(
                    filename.begin(),
                    std::find(filename.begin(), filename.end(), '.'));
                names.push_back(name);
            }
        }
        return names;
    }

}  // namespace index

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
    using score_table_type =
        compact_table<score_type, irk::vbyte_codec<score_type>, memory_view>;
    using size_table_type =
        compact_table<int32_t, irk::vbyte_codec<int32_t>, memory_view>;
    using array_stream = boost::iostreams::stream_buffer<
        boost::iostreams::basic_array_source<char>>;
    using score_tuple_type = score_tuple<
        memory_view,
        offset_table_type,
        score_table_type,
        score_table_type,
        score_table_type>;

    basic_inverted_index_view() = default;
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
          document_offsets_(data->document_offsets_view()),
          count_offsets_(data->count_offsets_view()),
          document_sizes_(data->document_sizes_view()),
          term_collection_frequencies_(
              data->term_collection_frequencies_view()),
          term_collection_occurrences_(
              data->term_collection_occurrences_view()),
          term_map_(std::move(load_lexicon(data->term_map_source()))),
          title_map_(std::move(load_lexicon(data->title_map_source()))),
          term_count_(term_collection_frequencies_.size())
    {
        EXPECTS(
            static_cast<ptrdiff_t>(document_offsets_.size()) == term_count_);
        EXPECTS(static_cast<ptrdiff_t>(count_offsets_.size()) == term_count_);

        for (const auto& [name, tuple] : data->scores_sources()) {
            score_tuple_type t{tuple.postings,
                               offset_table_type(tuple.offsets),
                               score_table_type(tuple.max_scores),
                               score_table_type(tuple.exp_values),
                               score_table_type(tuple.variances)};
            scores_.emplace(std::make_pair(name, t));
        }
        default_score_ = data->default_score();
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

    auto document_sizes() const { return document_sizes_; }

    auto documents(term_id_type term_id) const
    {
        EXPECTS(term_id < term_count_);
        auto length = term_collection_frequencies_[term_id];
        return index::block_document_list_view<document_codec_type>(
            select(term_id, document_offsets_, documents_view_), length);
    }

    auto documents(const std::string& term) const
    {
        if (auto id = term_id(term); id.has_value()) {
            return documents(id.value());
        }
        return index::block_document_list_view<document_codec_type>();
    }

    auto frequencies(term_id_type term_id) const
    {
        EXPECTS(term_id < term_count_);
        auto length = term_collection_frequencies_[term_id];
        return index::block_payload_list_view<frequency_type,
            frequency_codec_type>(
            select(term_id, count_offsets_, counts_view_), length);
    }

    auto frequencies(const std::string& term) const
    {
        if (auto id = term_id(term); id.has_value()) {
            return frequencies(id.value());
        }
        return index::
            block_payload_list_view<frequency_t, frequency_codec_type>();
    }

    auto scores(term_id_type term_id) const
    {
        EXPECTS(term_id < term_count_);
        auto length = term_collection_frequencies_[term_id];
        return index::block_payload_list_view<score_type, score_codec_type>(
            select(
                term_id,
                scores_.at(default_score_).offsets,
                scores_.at(default_score_).postings),
            length);
    }

    auto scores(const std::string& term) const
    {
        if (auto id = term_id(term); id.has_value()) {
            return scores(id.value());
        }
        return index::block_payload_list_view<score_type, score_codec_type>();
    }

    auto scores(term_id_type term_id, const std::string& score_fun_name) const
    {
        EXPECTS(term_id < term_count_);
        auto length = term_collection_frequencies_[term_id];
        return index::block_payload_list_view<score_type, score_codec_type>(
            select(
                term_id,
                scores_.at(score_fun_name).offsets,
                scores_.at(score_fun_name).postings),
            length);
    }

    auto score_expected_value(
        term_id_type term_id, const std::string& score_fun_name) const
    {
        EXPECTS(term_id < term_count_);
        return scores_.at(score_fun_name).exp_values[term_id];
    }

    auto score_expected_value(term_id_type term_id) const
    {
        EXPECTS(term_id < term_count_);
        return scores_.at(default_score_).exp_values[term_id];
    }

    auto score_variance(
        term_id_type term_id, const std::string& score_fun_name) const
    {
        EXPECTS(term_id < term_count_);
        return scores_.at(score_fun_name).variances[term_id];
    }

    auto score_variance(term_id_type term_id) const
    {
        EXPECTS(term_id < term_count_);
        return scores_.at(default_score_).variances[term_id];
    }

    auto postings(term_id_type term_id) const
    {
        EXPECTS(term_id < term_count_);
        auto length = term_collection_frequencies_[term_id];
        if (length == 0) {
            index::block_document_list_view<document_codec_type> documents;
            index::block_payload_list_view<frequency_type, frequency_codec_type>
                frequencies;
            return posting_list_view(documents, frequencies);
        }
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
        {
            index::block_document_list_view<document_codec_type> documents;
            index::block_payload_list_view<frequency_type, frequency_codec_type>
                frequencies;
            return posting_list_view(documents, frequencies);
        }
        return postings(*idopt);
    }

    auto scored_postings(term_id_type term_id) const
    {
        return scored_postings(term_id, default_score_);
    }

    auto scored_postings(term_id_type term_id, const std::string& score) const
    {
        EXPECTS(term_id < term_count_);
        if (scores_.empty()) {
            throw std::runtime_error("scores not loaded");
        }
        auto length = term_collection_frequencies_[term_id];
        if (length == 0) {
            index::block_document_list_view<document_codec_type> documents;
            index::block_payload_list_view<score_type, score_codec_type> scores;
            return posting_list_view(documents, scores);
        }
        auto documents = index::block_document_list_view<document_codec_type>(
            select(term_id, document_offsets_, documents_view_), length);
        auto scores =
            index::block_payload_list_view<score_type, score_codec_type>(
                select(
                    term_id,
                    scores_.at(score).offsets,
                    scores_.at(score).postings),
                length);
        return posting_list_view(documents, scores);
    }

    auto scored_postings(const std::string& term) const
    {
        auto idopt = term_id(term);
        if (not idopt.has_value())
        {
            index::block_document_list_view<document_codec_type> documents;
            index::block_payload_list_view<score_type, score_codec_type> scores;
            return posting_list_view(documents, scores);
        }
        return scored_postings(*idopt);
    }

    template<class Scorer>
    Scorer term_scorer(term_id_type term_id) const
    {
        if constexpr (std::is_same_v<  // NOLINT
                          Scorer,
                          score::bm25_scorer>) {
            return score::bm25_scorer(term_collection_frequencies_[term_id],
                document_count_,
                avg_document_size_);
        } else if constexpr (std::is_same_v<  // NOLINT
                                 Scorer,
                                 score::query_likelihood_scorer>) {
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

    const auto& term_collection_frequencies() const
    {
        return term_collection_frequencies_;
    }

    const auto& term_collection_occurrences() const
    {
        return term_collection_occurrences_;
    }

    /// Deprecated
    int32_t tdf(term_id_type term_id) const
    {
        return term_collection_frequencies_[term_id];
    }

    int32_t term_collection_frequency(term_id_type term_id) const
    {
        return term_collection_frequencies_[term_id];
    }

    int32_t term_collection_frequency(const std::string& term) const
    {
        if (auto id = term_id(term); id.has_value()) {
            return term_collection_frequencies_[*id];
        }
        return 0;
    }

    int32_t term_occurrences(term_id_type term_id) const
    {
        return term_collection_occurrences_[term_id];
    }

    int32_t term_occurrences(const std::string& term) const
    {
        if (auto id = term_id(term); id.has_value()) {
            return term_collection_occurrences_[*id];
        }
        return 0;
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
    const auto& score_data(const std::string& name) const
    {
        return scores_.at(name);
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

    std::vector<std::string> score_names() const
    {
        std::vector<std::string> names;
        for (const auto& entry : scores_) {
            names.push_back(entry.first);
        }
        return names;
    }

private:
    memory_view documents_view_;
    memory_view counts_view_;
    offset_table_type document_offsets_;
    offset_table_type count_offsets_;
    size_table_type document_sizes_;
    std::unordered_map<std::string, score_tuple_type> scores_;
    std::string default_score_;
    frequency_table_type term_collection_frequencies_;
    frequency_table_type term_collection_occurrences_;
    lexicon<hutucker_codec<char>, memory_view> term_map_;
    lexicon<hutucker_codec<char>, memory_view> title_map_;
    std::ptrdiff_t term_count_ = 0;
    std::ptrdiff_t document_count_ = 0;
    std::ptrdiff_t occurrences_count_ = 0;
    int block_size_ = 0;
    double avg_document_size_ = 0;

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

/// \returns All document lists for query terms in the preserved order.
inline auto query_documents(
    const irk::inverted_index_view& index,
    const std::vector<std::string>& query)
{
    using list_type = decltype(index.documents(std::declval<std::string>()));
    std::vector<list_type> documents;
    documents.reserve(query.size());
    irk::transform_range(
        query, std::back_inserter(documents), [&index](const auto& term) {
            return index.documents(term);
        });
    return documents;
}

/// \returns All frequency lists for query terms in the preserved order.
inline auto query_frequencies(
    const irk::inverted_index_view& index,
    const std::vector<std::string>& query)
{
    using list_type = decltype(index.frequencies(std::declval<std::string>()));
    std::vector<list_type> frequencies;
    frequencies.reserve(query.size());
    irk::transform_range(
        query, std::back_inserter(frequencies), [&index](const auto& term) {
            return index.frequencies(term);
        });
    return frequencies;
}

/// \returns All score lists for query terms in the preserved order.
inline auto query_scores(
    const irk::inverted_index_view& index,
    const std::vector<std::string>& query)
{
    using list_type = decltype(index.scores(std::declval<std::string>()));
    std::vector<list_type> scores;
    scores.reserve(query.size());
    irk::transform_range(
        query, std::back_inserter(scores), [&index](const auto& term) {
            return index.scores(term);
        });
    return scores;
}

inline auto query_postings(
    const irk::inverted_index_view& index,
    const std::vector<std::string>& query)
{
    using posting_list_type =
        decltype(index.postings(std::declval<std::string>()));
    std::vector<posting_list_type> postings;
    postings.reserve(query.size());
    for (const auto& term : query) {
        postings.push_back(index.postings(term));
    }
    return postings;
}

inline auto query_scored_postings(
    const irk::inverted_index_view& index,
    const std::vector<std::string>& query)
{
    using posting_list_type = decltype(
        index.scored_postings(std::declval<std::string>()));
    std::vector<posting_list_type> postings;
    postings.reserve(query.size());
    for (const auto& term : query) {
        postings.push_back(index.scored_postings(term));
    }
    return postings;
}

inline auto query_scored_postings(
    const irk::inverted_index_view& index,
    const std::vector<std::string>& query,
    const std::vector<std::function<double(
        irk::inverted_index_view::document_type,
        irk::inverted_index_view::frequency_type)>> score_fns)
{
    using posting_list_type =
        decltype(index.scored_postings(std::declval<std::string>())
                     .scored(score_fns[0]));
    std::vector<posting_list_type> postings;
    postings.reserve(query.size());
    auto score_iter = score_fns.begin();
    for (const auto& term : query) {
        postings.push_back(index.scored_postings(term).scored(*score_iter++));
    }
    return postings;
}

using score_fn_type = std::function<double(
    irk::inverted_index_view::document_type,
    irk::inverted_index_view::frequency_type)>;

inline auto query_scored_postings(
    const irk::inverted_index_view& index,
    const std::vector<std::string>& query,
    score::bm25_tag)
{
    auto unscored = query_postings(index, query);
    using scored_list_type =
        decltype(unscored[0].scored(std::declval<score_fn_type>()));
    std::vector<scored_list_type> postings;
    postings.reserve(query.size());
    for (term_id_t term_id = 0; term_id < irk::sgn(query.size()); ++term_id) {
        score::bm25_scorer scorer(
            index.term_collection_frequency(term_id),
            index.collection_size(),
            index.avg_document_size());
        postings.push_back(
            unscored[0].scored(score::BM25ScoreFn{index, std::move(scorer)}));
    }
    return postings;
}

inline auto query_scored_postings(
    const irk::inverted_index_view& index,
    const std::vector<std::string>& query,
    score::query_likelihood_tag)
{
    auto unscored = query_postings(index, query);
    using scored_list_type =
        decltype(unscored[0].scored(std::declval<score_fn_type>()));
    std::vector<scored_list_type> postings;
    postings.reserve(query.size());
    for (term_id_t term_id = 0; term_id < irk::sgn(query.size()); ++term_id) {
        score::query_likelihood_scorer scorer(
            index.term_occurrences(term_id), index.occurrences_count());
        postings.push_back(unscored[0].scored(
            score::QueryLikelihoodScoreFn{index, std::move(scorer)}));
    }
    return postings;
}

}  // namespace irk
