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
#include <nonstd/expected.hpp>

namespace irk {

using boost::filesystem::exists;
using boost::filesystem::path;
using boost::iostreams::mapped_file_source;

class inverted_index_disk_data_source {
public:
    explicit inverted_index_disk_data_source(path dir) : dir_(std::move(dir)) {}

    static nonstd::
        expected<inverted_index_disk_data_source, std::vector<std::string>>
        from(const path& dir, std::vector<std::string> score_names = {})
    {
        inverted_index_disk_data_source source(dir);
        source.documents_ = index::doc_ids_path(dir);
        source.counts_ = index::doc_counts_path(dir);
        source.document_offsets_ = index::doc_ids_off_path(dir);
        source.count_offsets_ = index::doc_counts_off_path(dir);
        source.term_collection_frequencies_ = index::term_doc_freq_path(dir);
        source.term_map_ = index::term_map_path(dir);
        source.title_map_ = index::title_map_path(dir);
        source.document_sizes_ = index::doc_sizes_path(dir);
        source.term_collection_occurrences_ = index::term_occurrences_path(dir);
        source.properties_ = index::properties_path(dir);

        std::vector<std::string> invalid_scores;
        for (const auto& score_name : score_names)
        {
            auto scores_path = dir / (score_name + ".scores");
            auto score_offsets_path = dir / (score_name + ".offsets");
            auto max_scores_path = dir / (score_name + ".maxscore");
            if (exists(scores_path) && exists(score_offsets_path)
                && exists(max_scores_path)) {
                source.scores_[score_name] = {dir / (score_name + ".scores"),
                                              dir / (score_name + ".offsets"),
                                              dir / (score_name + ".maxscore")};
            } else {
                invalid_scores.push_back(score_name);
            }
        }
        if (not invalid_scores.empty()) {
            return nonstd::make_unexpected(invalid_scores);
        }
        if (not score_names.empty()) {
            source.default_score_ = score_names[0];
        }
        return source;
    }

    path dir() { return dir_; }

    memory_view documents_view() const { return make_memory_view(documents_); }

    memory_view counts_view() const { return make_memory_view(counts_); }

    memory_view document_offsets_view() const
    {
        return make_memory_view(document_offsets_);
    }

    memory_view count_offsets_view() const
    {
        return make_memory_view(count_offsets_);
    }

    memory_view term_collection_frequencies_view() const
    {
        return make_memory_view(term_collection_frequencies_);
    }

    memory_view term_collection_occurrences_view() const
    {
        return make_memory_view(term_collection_occurrences_);
    }

    memory_view term_map_source() const { return make_memory_view(term_map_); }

    memory_view title_map_source() const
    {
        return make_memory_view(title_map_);
    }

    memory_view document_sizes_view() const
    {
        return make_memory_view(document_sizes_);
    }

    memory_view properties_view() const
    {
        return make_memory_view(properties_);
    }

    std::optional<memory_view> scores_source() const
    {
        return not scores_.empty()
            ? std::make_optional<memory_view>(
                  make_memory_view(scores_.at(default_score_).postings))
            : std::nullopt;
    }

    std::optional<memory_view> score_offset_source() const
    {
        return not scores_.empty()
            ? std::make_optional<memory_view>(
                  make_memory_view(scores_.at(default_score_).offsets))
            : std::nullopt;
    }

    std::optional<memory_view> max_scores_source() const
    {
        return not scores_.empty()
            ? std::make_optional<memory_view>(
                  make_memory_view(scores_.at(default_score_).max_scores))
            : std::nullopt;
    }

    nonstd::expected<score_tuple<memory_view>, std::string>
    scores_source(const std::string& name) const
    {
        if (auto pos = scores_.find(name); pos != scores_.end()) {
            return score_tuple<memory_view>{
                make_memory_view(pos->second.postings),
                make_memory_view(pos->second.offsets),
                make_memory_view(pos->second.max_scores)};
        }
        return nonstd::make_unexpected("requested score function not found");
    }

    std::unordered_map<std::string, score_tuple<memory_view>>
    scores_sources() const
    {
        std::unordered_map<std::string, score_tuple<memory_view>> view_map;
        for (const auto& entry : scores_) {
            const auto& key = entry.first;
            view_map[key] = *scores_source(key);
        };
        return view_map;
    }
    const std::string& default_score() const { return default_score_; }

private:
    path dir_;
    path documents_;
    path counts_;
    path document_offsets_;
    path count_offsets_;
    path term_collection_frequencies_;
    path term_collection_occurrences_;
    path term_map_;
    path title_map_;
    path document_sizes_;
    path properties_;
    std::unordered_map<std::string, score_tuple<path>> scores_;
    std::string default_score_ = "";
};

class inverted_index_inmemory_data_source {
public:
    explicit inverted_index_inmemory_data_source(path dir)
        : dir_(dir)
    {}

    static nonstd::
        expected<inverted_index_inmemory_data_source, std::vector<std::string>>
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

        std::vector<std::string> invalid_scores;
        for (const std::string& score_name : score_names)
        {
            auto scores_path = dir / (score_name + ".scores");
            auto score_offsets_path = dir / (score_name + ".offsets");
            auto max_scores_path = dir / (score_name + ".maxscore");
            if (exists(scores_path) && exists(score_offsets_path)
                && exists(max_scores_path))
            {
                std::vector<char> scores;
                std::vector<char> score_offsets;
                std::vector<char> max_scores;
                load_data(scores_path, scores);
                load_data(score_offsets_path, score_offsets);
                load_data(max_scores_path, max_scores);
                source.scores_[score_name] = {std::move(scores),
                                              std::move(score_offsets),
                                              std::move(max_scores)};
            } else {
                invalid_scores.push_back(score_name);
            }
        }
        if (not invalid_scores.empty()) {
            return nonstd::make_unexpected(invalid_scores);
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

    nonstd::expected<score_tuple<memory_view>, std::string>
    scores_source(const std::string& name) const
    {
        if (auto pos = scores_.find(name); pos != scores_.end()) {
            return score_tuple<memory_view>{
                make_memory_view(pos->second.postings),
                make_memory_view(pos->second.offsets),
                make_memory_view(pos->second.max_scores)};
        }
        return nonstd::make_unexpected("requested score function not found");
    }

    std::unordered_map<std::string, score_tuple<memory_view>>
    scores_sources() const
    {
        std::unordered_map<std::string, score_tuple<memory_view>> view_map;
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
    std::unordered_map<std::string, score_tuple<std::vector<char>>> scores_;
    std::string default_score_ = "";
};

class inverted_index_mapped_data_source {

public:
    explicit inverted_index_mapped_data_source(path dir) : dir_(std::move(dir))
    {}
    static nonstd::
        expected<inverted_index_mapped_data_source, std::vector<std::string>>
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

        std::vector<std::string> invalid_scores;
        for (const std::string& score_name : score_names) {
            auto scores_path = dir / (score_name + ".scores");
            auto score_offsets_path = dir / (score_name + ".offsets");
            auto max_scores_path = dir / (score_name + ".maxscore");
            if (exists(scores_path) && exists(score_offsets_path)
                && exists(max_scores_path))
            {
                source.scores_[score_name] = {
                    mapped_file_source(scores_path),
                    mapped_file_source(score_offsets_path),
                    mapped_file_source(max_scores_path)};
            } else {
                invalid_scores.push_back(score_name);
            }
        }
        if (not invalid_scores.empty()) {
            return nonstd::make_unexpected(invalid_scores);
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

    nonstd::expected<score_tuple<memory_view>, std::string>
    scores_source(const std::string& name) const
    {
        if (auto pos = scores_.find(name); pos != scores_.end()) {
            return score_tuple<memory_view>{
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

    std::unordered_map<std::string, score_tuple<memory_view>>
    scores_sources() const
    {
        std::unordered_map<std::string, score_tuple<memory_view>> view_map;
        for (const auto& entry : scores_) {
            const auto& [key, sources] = entry;
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
    std::unordered_map<std::string, score_tuple<mapped_file_source>> scores_;
    std::string default_score_ = "";
};

};  // namespace irk
