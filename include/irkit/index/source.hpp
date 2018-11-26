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

#include <optional>

#include <boost/filesystem.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <fmt/format.h>

#include <irkit/index.hpp>
#include <irkit/value.hpp>
#include <nonstd/expected.hpp>

namespace irk {

using boost::filesystem::exists;
using boost::filesystem::path;
using boost::iostreams::mapped_file_source;

namespace source::detail {

    inline std::string
    invalid_scores_message(const std::vector<std::string>& names)
    {
        std::ostringstream os;
        os << "Invalid score names:";
        for (const std::string& name : names) {
            os << " " << name;
        }
        return os.str();
    }

}  // namespace source::detail

class inverted_index_inmemory_data_source {
public:
    explicit inverted_index_inmemory_data_source(path dir)
        : dir_(std::move(dir))
    {}

    static nonstd::expected<inverted_index_inmemory_data_source, std::string>
    from(const path& dir, std::vector<std::string> score_names = {})
    {
        using io::load_data;
        inverted_index_inmemory_data_source source(dir);
        load_data(index::doc_ids_path(dir), source.documents_);
        load_data(index::doc_counts_path(dir), source.counts_);
        load_data(index::doc_ids_off_path(dir), source.document_offsets_);
        load_data(index::doc_counts_off_path(dir), source.count_offsets_);
        load_data(
            index::term_doc_freq_path(dir),
            source.term_collection_frequencies_);
        load_data(index::term_map_path(dir), source.term_map_);
        load_data(index::title_map_path(dir), source.title_map_);
        load_data(index::doc_sizes_path(dir), source.document_sizes_);
        load_data(
            index::term_occurrences_path(dir),
            source.term_collection_occurrences_);
        load_data(index::properties_path(dir), source.properties_);

        source.score_stats_ = index::transform_score_stats_map(
            index::find_score_stats_paths(dir), [](const auto& path) {
                std::vector<char> data;
                load_data(path, data);
                return data;
            });

        std::vector<std::string> invalid_scores;
        for (const std::string& score_name : score_names)
        {
            auto score_paths = index::score_paths(dir, score_name);
            if (exists(score_paths.postings) && exists(score_paths.offsets)
                && exists(score_paths.max_scores))
            {
                std::vector<char> scores;
                std::vector<char> score_offsets;
                std::vector<char> max_scores;
                std::vector<char> exp_values;
                std::vector<char> variances;
                load_data(score_paths.postings, scores);
                load_data(score_paths.offsets, score_offsets);
                load_data(score_paths.max_scores, max_scores);
                source.scores_[score_name] = {std::move(scores),
                                              std::move(score_offsets),
                                              std::move(max_scores)};
            } else {
                invalid_scores.push_back(score_name);
            }
        }
        if (not invalid_scores.empty()) {
            return nonstd::make_unexpected(
                source::detail::invalid_scores_message(invalid_scores));
        }
        if (not score_names.empty()) {
            source.default_score_ = score_names[0];
        }
        return source;
    }

    path dir() { return dir_; }

    memory_view documents_view() const
    {
        return make_memory_view(documents_.data(), documents_.size());
    }

    memory_view counts_view() const
    {
        return make_memory_view(counts_.data(), counts_.size());
    }

    memory_view document_offsets_view() const
    {
        return make_memory_view(
            document_offsets_.data(), document_offsets_.size());
    }

    memory_view count_offsets_view() const
    {
        return make_memory_view(count_offsets_.data(), count_offsets_.size());
    }

    memory_view term_collection_frequencies_view() const
    {
        return make_memory_view(term_collection_frequencies_.data(),
            term_collection_frequencies_.size());
    }

    memory_view term_collection_occurrences_view() const
    {
        return make_memory_view(term_collection_occurrences_.data(),
            term_collection_occurrences_.size());
    }

    memory_view term_map_source() const
    {
        return make_memory_view(term_map_.data(), term_map_.size());
    }

    memory_view title_map_source() const
    {
        return make_memory_view(title_map_.data(), title_map_.size());
    }

    memory_view document_sizes_view() const
    {
        return make_memory_view(document_sizes_.data(), document_sizes_.size());
    }

    memory_view properties_view() const
    {
        return make_memory_view(properties_.data(), properties_.size());
    }

    template<class ScoreTag, class UnaryFun>
    std::optional<memory_view>
    score_stat_view(ScoreTag tag, UnaryFun accessor) const
    {
        static_assert(
            std::is_base_of<score::scoring_function_tag, ScoreTag>(),
            "invalid score tag");
        auto stat = accessor(score_stats_.at(static_cast<std::string>(tag)));
        if (stat) {
            return make_memory_view(stat.value());
        } else {
            return std::nullopt;
        }
    }

    template<class ScoreTag>
    std::optional<memory_view> score_max_view(ScoreTag tag) const
    {
        return score_stat_view(
            tag, [](const auto& stats) { return stats.max; });
    }

    template<class ScoreTag>
    std::optional<memory_view> score_mean_view(ScoreTag tag) const
    {
        return score_stat_view(
            tag, [](const auto& stats) { return stats.mean; });
    }

    template<class ScoreTag>
    std::optional<memory_view> score_var_view(ScoreTag tag) const
    {
        return score_stat_view(
            tag, [](const auto& stats) { return stats.var; });
    }

    auto score_stats_views() const
    {
        return index::transform_score_stats_map(
            score_stats_, [](const auto& vec) {
                return make_memory_view(vec.data(), vec.size());
            });
    }

    std::optional<memory_view> scores_source() const
    {
        if (scores_.empty()) { return std::nullopt; }
        return std::make_optional(make_memory_view(
            scores_.at(default_score_).postings.data(),
            scores_.at(default_score_).postings.size()));
    }

    std::optional<memory_view> score_offset_source() const
    {
        if (scores_.empty()) { return std::nullopt; }
        return std::make_optional(make_memory_view(
            scores_.at(default_score_).offsets.data(),
            scores_.at(default_score_).offsets.size()));
    }

    std::optional<memory_view> max_scores_source() const
    {
        if (scores_.empty()) { return std::nullopt; }
        return std::make_optional(make_memory_view(
            scores_.at(default_score_).max_scores.data(),
            scores_.at(default_score_).max_scores.size()));
    }

    nonstd::expected<quantized_score_tuple<memory_view>, std::string>
    scores_source(const std::string& name) const
    {
        if (auto pos = scores_.find(name); pos != scores_.end()) {
            return quantized_score_tuple<memory_view>{
                make_memory_view(pos->second.postings),
                make_memory_view(pos->second.offsets),
                make_memory_view(pos->second.max_scores)};
        }
        return nonstd::make_unexpected("requested score function not found");
    }

    std::unordered_map<std::string, quantized_score_tuple<memory_view>>
    scores_sources() const
    {
        std::unordered_map<std::string, quantized_score_tuple<memory_view>>
            view_map;
        for (const auto& entry : scores_) {
            const auto& key = entry.first;
            view_map[key] = *scores_source(key);
        };
        return view_map;
    }
    const std::string& default_score() const { return default_score_; }

private:
    path dir_;
    std::vector<char> documents_;
    std::vector<char> counts_;
    std::vector<char> document_offsets_;
    std::vector<char> count_offsets_;
    std::vector<char> term_collection_frequencies_;
    std::vector<char> term_collection_occurrences_;
    std::vector<char> term_map_;
    std::vector<char> title_map_;
    std::vector<char> document_sizes_;
    std::vector<char> properties_;
    index::ScoreStatsMap<std::vector<char>> score_stats_{};
    std::unordered_map<std::string, quantized_score_tuple<std::vector<char>>>
        scores_;
    std::string default_score_ = "";
};

class inverted_index_mapped_data_source {

public:
    explicit inverted_index_mapped_data_source(path dir) : dir_(std::move(dir))
    {}
    static nonstd::expected<inverted_index_mapped_data_source, std::string>
    from(const path& dir, std::vector<std::string> score_names = {})
    {
        inverted_index_mapped_data_source source(dir);
        io::enforce_exist(index::doc_ids_path(dir));
        io::enforce_exist(index::doc_counts_path(dir));
        io::enforce_exist(index::doc_ids_off_path(dir));
        io::enforce_exist(index::doc_counts_off_path(dir));
        io::enforce_exist(index::term_doc_freq_path(dir));
        io::enforce_exist(index::term_map_path(dir));
        io::enforce_exist(index::title_map_path(dir));
        io::enforce_exist(index::doc_sizes_path(dir));
        io::enforce_exist(index::term_occurrences_path(dir));
        io::enforce_exist(index::properties_path(dir));

        source.documents_.open(index::doc_ids_path(dir));
        source.counts_.open(index::doc_counts_path(dir));
        source.document_offsets_.open(index::doc_ids_off_path(dir));
        source.count_offsets_.open(index::doc_counts_off_path(dir));
        source.term_collection_frequencies_.open(
            index::term_doc_freq_path(dir));
        source.term_map_.open(index::term_map_path(dir));
        source.title_map_.open(index::title_map_path(dir));
        source.document_sizes_.open(index::doc_sizes_path(dir));
        source.term_collection_occurrences_.open(
            index::term_occurrences_path(dir));
        source.properties_.open(index::properties_path(dir));

        source.score_stats_ = index::transform_score_stats_map(
            index::find_score_stats_paths(dir),
            [](const auto& path) { return mapped_file_source{path}; });

        std::vector<std::string> invalid_scores;
        for (const std::string& score_name : score_names) {
            auto score_paths = index::score_paths(dir, score_name);
            if (exists(score_paths.postings) && exists(score_paths.offsets)
                && exists(score_paths.max_scores))
            {
                source.scores_[score_name] = {
                    mapped_file_source(score_paths.postings),
                    mapped_file_source(score_paths.offsets),
                    mapped_file_source(score_paths.max_scores)};
            } else {
                invalid_scores.push_back(score_name);
            }
        }
        if (not invalid_scores.empty()) {
            return nonstd::make_unexpected(
                source::detail::invalid_scores_message(invalid_scores));
        }
        if (not score_names.empty()) {
            source.default_score_ = score_names[0];
        }
        return source;
    }

    path dir() { return dir_; }

    memory_view documents_view() const
    {
        return make_memory_view(documents_.data(), documents_.size());
    }

    memory_view counts_view() const
    {
        return make_memory_view(counts_.data(), counts_.size());
    }

    memory_view document_offsets_view() const
    {
        return make_memory_view(
            document_offsets_.data(), document_offsets_.size());
    }

    memory_view count_offsets_view() const
    {
        return make_memory_view(count_offsets_.data(), count_offsets_.size());
    }

    memory_view term_collection_frequencies_view() const
    {
        return make_memory_view(term_collection_frequencies_.data(),
            term_collection_frequencies_.size());
    }

    memory_view term_collection_occurrences_view() const
    {
        return make_memory_view(term_collection_occurrences_.data(),
            term_collection_occurrences_.size());
    }

    memory_view term_map_source() const
    {
        return make_memory_view(term_map_.data(), term_map_.size());
    }

    memory_view title_map_source() const
    {
        return make_memory_view(title_map_.data(), title_map_.size());
    }

    memory_view document_sizes_view() const
    {
        return make_memory_view(document_sizes_.data(), document_sizes_.size());
    }

    memory_view properties_view() const
    {
        return make_memory_view(properties_.data(), properties_.size());
    }

    template<class ScoreTag, class UnaryFun>
    std::optional<memory_view>
    score_stat_view(ScoreTag tag, UnaryFun accessor) const
    {
        static_assert(
            std::is_base_of<score::scoring_function_tag, ScoreTag>(),
            "invalid score tag");
        auto stat = accessor(score_stats_.at(static_cast<std::string>(tag)));
        if (stat) {
            return make_memory_view(stat->data(), stat->size());
        } else {
            return std::nullopt;
        }
    }

    template<class ScoreTag>
    std::optional<memory_view> score_max_view(ScoreTag tag) const
    {
        return score_stat_view(
            tag, [](const auto& stats) { return stats.max; });
    }

    template<class ScoreTag>
    std::optional<memory_view> score_mean_view(ScoreTag tag) const
    {
        return score_stat_view(
            tag, [](const auto& stats) { return stats.mean; });
    }

    template<class ScoreTag>
    std::optional<memory_view> score_var_view(ScoreTag tag) const
    {
        return score_stat_view(
            tag, [](const auto& stats) { return stats.var; });
    }

    auto score_stats_views() const
    {
        return index::transform_score_stats_map(
            score_stats_, [](const auto& file) {
                return make_memory_view(file.data(), file.size());
            });
    }

    std::optional<memory_view> scores_source() const
    {
        if (scores_.empty()) { return std::nullopt; }
        return std::make_optional(make_memory_view(
            scores_.at(default_score_).postings.data(),
            scores_.at(default_score_).postings.size()));
    }

    std::optional<memory_view> score_offset_source() const
    {
        if (scores_.empty()) { return std::nullopt; }
        return std::make_optional(make_memory_view(
            scores_.at(default_score_).offsets.data(),
            scores_.at(default_score_).offsets.size()));
    }

    std::optional<memory_view> max_scores_source() const
    {
        if (scores_.empty()) { return std::nullopt; }
        return std::make_optional(make_memory_view(
            scores_.at(default_score_).max_scores.data(),
            scores_.at(default_score_).max_scores.size()));
    }

    nonstd::expected<quantized_score_tuple<memory_view>, std::string>
    scores_source(const std::string& name) const
    {
        if (auto pos = scores_.find(name); pos != scores_.end()) {
            return quantized_score_tuple<memory_view>{
                make_memory_view(
                    pos->second.postings.data(), pos->second.postings.size()),
                make_memory_view(
                    pos->second.offsets.data(), pos->second.offsets.size()),
                make_memory_view(
                    pos->second.max_scores.data(),
                    pos->second.max_scores.size())};
        }
        return nonstd::make_unexpected("requested score function not found");
    }

    std::unordered_map<std::string, quantized_score_tuple<memory_view>>
    scores_sources() const
    {
        std::unordered_map<std::string, quantized_score_tuple<memory_view>>
            view_map;
        for (const auto& entry : scores_) {
            const auto& key = entry.first;
            view_map[key] = *scores_source(key);
        };
        return view_map;
    }
    const std::string& default_score() const { return default_score_; }

private:
    path dir_;
    mapped_file_source documents_;
    mapped_file_source counts_;
    mapped_file_source document_offsets_;
    mapped_file_source count_offsets_;
    mapped_file_source term_collection_frequencies_;
    mapped_file_source term_collection_occurrences_;
    mapped_file_source term_map_;
    mapped_file_source title_map_;
    mapped_file_source document_sizes_;
    mapped_file_source properties_;
    index::ScoreStatsMap<mapped_file_source> score_stats_{};
    std::unordered_map<std::string, quantized_score_tuple<mapped_file_source>>
        scores_;
    std::string default_score_ = "";
};

template<class T>
struct PropertySource {
    auto properties() const {
        return index::Properties::read(static_cast<T const&>(*this).dir());
    }
};

template<class T>
struct MappedTablesSource {
public:
    MappedTablesSource(path const& dir)
    {
        io::enforce_exist(index::term_doc_freq_path(dir));
        io::enforce_exist(index::term_occurrences_path(dir));
        io::enforce_exist(index::term_map_path(dir));
        term_collection_frequencies_.open(index::term_doc_freq_path(dir));
        term_collection_occurrences_.open(index::term_occurrences_path(dir));
        term_map_.open(index::term_map_path(dir));
    }

    memory_view term_collection_frequencies_view() const
    {
        return make_memory_view(term_collection_frequencies_.data(),
            term_collection_frequencies_.size());
    }

    memory_view term_collection_occurrences_view() const
    {
        return make_memory_view(term_collection_occurrences_.data(),
            term_collection_occurrences_.size());
    }

    memory_view term_map_view() const
    {
        return make_memory_view(term_map_.data(), term_map_.size());
    }

private:
    mapped_file_source term_collection_frequencies_{};
    mapped_file_source term_collection_occurrences_{};
    mapped_file_source term_map_{};
};

template<class ShardSource>
class Index_Cluster_Data_Source
    : public PropertySource<Index_Cluster_Data_Source<ShardSource>>,
      public MappedTablesSource<Index_Cluster_Data_Source<ShardSource>> {
    using document_type = irk::index::document_t;
    using Self = Index_Cluster_Data_Source<ShardSource>;
    using size_type = std::int32_t;

public:
    [[nodiscard]] static auto from(const path& dir, std::vector<std::string> score_names = {})
        -> std::shared_ptr<const Index_Cluster_Data_Source>
    {
        int32_t shard_count = irtl::value(index::Properties::read(dir).shard_count,
                                          "not a cluster: shard count undefined");
        irk::vmap<ShardId, ShardSource> shards;
        vmap<ShardId, vmap<document_type>> reverse_mapping;
        for (auto shard : iter::range(shard_count)) {
            auto shard_dir = dir / fmt::format("{:03d}", shard);
            shards.push_back(irtl::value(ShardSource::from(shard_dir, score_names)));
            reverse_mapping.push_back(io::read_vmap<document_type>(shard_dir / "reverse.mapping"));
        }
        return std::make_shared<Index_Cluster_Data_Source<ShardSource>>(
            Index_Cluster_Data_Source(dir, std::move(shards), std::move(reverse_mapping)));
    }

    [[nodiscard]] auto shard_count() const -> size_type { return irk::sgnd(shards_.size()); }
    [[nodiscard]] auto const& shards() const noexcept { return shards_; }
    [[nodiscard]] auto const& reverse_mapping() const noexcept { return reverse_mapping_; }
    [[nodiscard]] auto const& reverse_mapping(ShardId shard) const
    {
        return reverse_mapping_[shard];
    }
    [[nodiscard]] auto const& dir() const noexcept { return dir_; }

private:
    explicit Index_Cluster_Data_Source(path dir,
                                       vmap<ShardId, ShardSource>&& shards,
                                       vmap<ShardId, vmap<index::document_t>>&& reverse_mapping)
        : MappedTablesSource<Self>(dir),
          dir_(std::move(dir)),
          shards_(shards),
          reverse_mapping_(reverse_mapping)
    {}

    path dir_;
    vmap<ShardId, ShardSource> shards_;
    vmap<index::document_t, ShardId> shard_mapping_;
    vmap<ShardId, vmap<index::document_t>> reverse_mapping_;
};

}  // namespace irk
