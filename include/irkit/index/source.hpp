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
#include <irkit/vector.hpp>
#include <nonstd/expected.hpp>

namespace irk {

using boost::filesystem::exists;
using boost::filesystem::path;
using boost::iostreams::mapped_file_source;
using ir::Vector;

namespace source::detail {

    inline std::string invalid_scores_message(const std::vector<std::string>& names)
    {
        std::ostringstream os;
        os << "Invalid score names:";
        for (const std::string& name : names) {
            os << " " << name;
        }
        return os.str();
    }

}  // namespace source::detail

#define REGISTER_MEMORY_SOURCE(field)                                                              \
    Memory_Source field;                                                                           \
    [[nodiscard]] memory_view field##_view() const { return Index_Source::make_view(field); }

template<typename Index_Source, typename Memory_Source>
class Inverted_Index_Source {
public:
    using pointer = std::shared_ptr<Index_Source const>;

    Inverted_Index_Source(path dir) : dir_(dir) {}

    [[nodiscard]] static auto from(path dir, std::vector<std::string> const& score_names = {})
        -> nonstd::expected<pointer, std::string>
    {
        auto source = std::make_shared<Index_Source>(dir);
        Index_Source::init(source->documents, index::doc_ids_path(dir));
        Index_Source::init(source->counts, index::doc_counts_path(dir));
        Index_Source::init(source->document_offsets, index::doc_ids_off_path(dir));
        Index_Source::init(source->count_offsets, index::doc_counts_off_path(dir));
        Index_Source::init(source->term_collection_frequencies, index::term_doc_freq_path(dir));
        Index_Source::init(source->term_collection_occurrences, index::term_occurrences_path(dir));
        Index_Source::init(source->term_map, index::term_map_path(dir));
        Index_Source::init(source->title_map, index::title_map_path(dir));
        Index_Source::init(source->document_sizes, index::doc_sizes_path(dir));
        Index_Source::init(source->properties, index::properties_path(dir));

        source->score_stats = index::transform_score_stats_map(
            index::find_score_stats_paths(dir),
            [](auto const& path) { return Index_Source::init(path); });

        std::vector<std::string> invalid_scores;
        for (const std::string& score_name : score_names) {
            auto score_paths = index::score_paths(dir, score_name);
            if (exists(score_paths.postings) && exists(score_paths.offsets)
                && exists(score_paths.max_scores))
            {
                source->scores_[score_name] = {Index_Source::init(score_paths.postings),
                                               Index_Source::init(score_paths.offsets),
                                               Index_Source::init(score_paths.max_scores)};
            } else {
                invalid_scores.push_back(score_name);
            }
        }
        if (not invalid_scores.empty()) {
            return nonstd::make_unexpected(source::detail::invalid_scores_message(invalid_scores));
        }
        if (not score_names.empty()) {
            source->default_score_ = score_names[0];
        }
        return source;
    }
    path dir_;
    [[nodiscard]] auto dir() const -> path const& { return dir_; }

    REGISTER_MEMORY_SOURCE(documents);
    REGISTER_MEMORY_SOURCE(counts);
    REGISTER_MEMORY_SOURCE(document_offsets);
    REGISTER_MEMORY_SOURCE(count_offsets);
    REGISTER_MEMORY_SOURCE(term_collection_frequencies);
    REGISTER_MEMORY_SOURCE(term_collection_occurrences);
    REGISTER_MEMORY_SOURCE(term_map);
    REGISTER_MEMORY_SOURCE(title_map);
    REGISTER_MEMORY_SOURCE(document_sizes);
    REGISTER_MEMORY_SOURCE(properties);

    index::ScoreStatsMap<Memory_Source> score_stats;
    [[nodiscard]] auto score_stats_views() const
    {
        return index::transform_score_stats_map(
            score_stats, [](auto const& stat) { return Index_Source::make_view(stat); });
    }

    std::unordered_map<std::string, quantized_score_tuple<Memory_Source>> scores_{};
    std::string default_score_{};

    [[nodiscard]] auto scores_source(std::string const& name) const
        -> nonstd::expected<quantized_score_tuple<memory_view>, std::string>
    {
        if (auto pos = scores_.find(name); pos != scores_.end()) {
            return quantized_score_tuple<memory_view>{
                Index_Source::make_view(pos->second.postings),
                Index_Source::make_view(pos->second.offsets),
                Index_Source::make_view(pos->second.max_scores)};
        }
        return nonstd::make_unexpected("requested score function not found");
    }

    [[nodiscard]] auto
    scores_sources() const -> std::unordered_map<std::string, quantized_score_tuple<memory_view>>
    {
        std::unordered_map<std::string, quantized_score_tuple<memory_view>> view_map;
        for (const auto& entry : scores_) {
            const auto& key = entry.first;
            view_map[key] = *scores_source(key);
        };
        return view_map;
    }
    [[nodiscard]] auto default_score() const -> std::string const& { return default_score_; }
};

class Inverted_Index_Mapped_Source
    : public Inverted_Index_Source<Inverted_Index_Mapped_Source, mapped_file_source> {
public:
    Inverted_Index_Mapped_Source(path const& dir)
        : Inverted_Index_Source<Inverted_Index_Mapped_Source, mapped_file_source>(dir)
    {}
    [[nodiscard]] static memory_view make_view(mapped_file_source const& memory_source)
    {
        return make_memory_view(memory_source.data(), memory_source.size());
    }
    [[nodiscard]] static mapped_file_source init(path const& file_path)
    {
        io::enforce_exist(file_path);
        return mapped_file_source(file_path);
    }
    static void init(mapped_file_source& source, path const& file_path)
    {
        io::enforce_exist(file_path);
        source.open(file_path);
    }
};

class Inverted_Index_In_Memory_Source
    : public Inverted_Index_Source<Inverted_Index_In_Memory_Source, std::vector<char>> {
public:
    Inverted_Index_In_Memory_Source(path const& dir)
        : Inverted_Index_Source<Inverted_Index_In_Memory_Source, std::vector<char>>(dir)
    {}
    [[nodiscard]] static memory_view make_view(std::vector<char> const& memory_source)
    {
        return make_memory_view(memory_source.data(), memory_source.size());
    }
    [[nodiscard]] static std::vector<char> init(path const& file_path)
    {
        io::enforce_exist(file_path);
        std::vector<char> vec;
        io::load_data(file_path, vec);
        return vec;
    }
    static void init(std::vector<char>& source, path const& file_path)
    {
        io::enforce_exist(file_path);
        io::load_data(file_path, source);
    }
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

template<class T>
struct Mapped_Score_Statistics_Source {
public:
    Mapped_Score_Statistics_Source(path const& dir)
    {
        score_stats_ = index::transform_score_stats_map(
            index::find_score_stats_paths(dir),
            [](const auto& path) { return mapped_file_source{path}; });
    }

    [[nodiscard]] auto score_stats_views() const
    {
        return index::transform_score_stats_map(
            score_stats_, [](const auto& vec) { return make_memory_view(vec.data(), vec.size()); });
    }

private:
    index::ScoreStatsMap<mapped_file_source> score_stats_{};
};

template<class ShardSource>
class Index_Cluster_Data_Source
    : public PropertySource<Index_Cluster_Data_Source<ShardSource>>,
      public MappedTablesSource<Index_Cluster_Data_Source<ShardSource>>,
      public Mapped_Score_Statistics_Source<Index_Cluster_Data_Source<ShardSource>> {
    using document_type = irk::index::document_t;
    using Self = Index_Cluster_Data_Source<ShardSource>;
    using size_type = std::int32_t;
    using shard_vector = Vector<ShardId, std::shared_ptr<ShardSource const>>;

public:
    Index_Cluster_Data_Source() = default;
    Index_Cluster_Data_Source(path dir, shard_vector&& shards)
        : MappedTablesSource<Self>(dir),
          Mapped_Score_Statistics_Source<Self>(dir),
          dir_(std::move(dir)),
          shards_(shards)
    {}
    [[nodiscard]] static auto from(const path& dir, std::vector<std::string> score_names = {})
        -> std::shared_ptr<Index_Cluster_Data_Source const>
    {
        int32_t shard_count = irtl::value(index::Properties::read(dir).shard_count,
                                          "not a cluster: shard count undefined");
        shard_vector shards;
        for (auto shard : iter::range(shard_count)) {
            auto shard_dir = dir / fmt::format("{:03d}", shard);
            shards.push_back(irtl::value(ShardSource::from(shard_dir, score_names)));
        }
        return std::make_shared<Index_Cluster_Data_Source<ShardSource>>(dir, std::move(shards));
    }

    [[nodiscard]] auto shard_count() const -> size_type { return irk::sgnd(shards_.size()); }
    [[nodiscard]] auto const& shards() const noexcept { return shards_; }
    [[nodiscard]] auto const& dir() const noexcept { return dir_; }

private:
    path dir_;
    shard_vector shards_;
    Vector<index::document_t, ShardId> shard_mapping_;
};

}  // namespace irk
