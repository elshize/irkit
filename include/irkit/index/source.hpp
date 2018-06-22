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

#include <optional>

#include <boost/filesystem.hpp>
#include <boost/iostreams/device/mapped_file.hpp>

#include <irkit/index.hpp>

namespace irk {

using boost::filesystem::exists;
using boost::filesystem::path;
using boost::iostreams::mapped_file_source;

class inverted_index_disk_data_source {
public:
    explicit inverted_index_disk_data_source(path dir,
        std::optional<std::string> score_name = std::nullopt)
        : dir_(dir)
    {
        documents_ = index::doc_ids_path(dir);
        counts_ = index::doc_counts_path(dir);
        document_offsets_ = index::doc_ids_off_path(dir);
        count_offsets_ = index::doc_counts_off_path(dir);
        term_collection_frequencies_ = index::term_doc_freq_path(dir);
        term_map_ = index::term_map_path(dir);
        title_map_ = index::title_map_path(dir);
        document_sizes_ = index::doc_sizes_path(dir);
        term_collection_occurrences_ = index::term_occurrences_path(dir);
        properties_ = index::properties_path(dir);

        if (score_name.has_value())
        {
            scores_ = std::make_optional(dir / (*score_name + ".scores"));
            score_offsets_ = std::make_optional(
                dir / (*score_name + ".offsets"));
        }
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
        if (!scores_.has_value()) { return std::nullopt; }
        return make_memory_view(*scores_);
    }

    std::optional<memory_view> score_offset_source() const
    {
        if (!score_offsets_.has_value()) { return std::nullopt; }
        return make_memory_view(*score_offsets_);
    }

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
    std::optional<path> scores_;
    std::optional<path> score_offsets_;
};

class inverted_index_inmemory_data_source {
public:
    explicit inverted_index_inmemory_data_source(
        path dir, std::optional<std::string> score_name = std::nullopt)
        : dir_(dir)
    {
        io::load_data(index::doc_ids_path(dir), documents_);
        io::load_data(index::doc_counts_path(dir), counts_);
        io::load_data(index::doc_ids_off_path(dir), document_offsets_);
        io::load_data(index::doc_counts_off_path(dir), count_offsets_);
        io::load_data(
            index::term_doc_freq_path(dir), term_collection_frequencies_);
        io::load_data(index::term_map_path(dir), term_map_);
        io::load_data(index::title_map_path(dir), title_map_);
        io::load_data(index::doc_sizes_path(dir), document_sizes_);
        io::load_data(
            index::term_occurrences_path(dir), term_collection_occurrences_);
        io::load_data(index::properties_path(dir), properties_);

        if (score_name.has_value())
        {
            auto scores_path = dir / (*score_name + ".scores");
            auto score_offsets_path = dir / (*score_name + ".offsets");
            if (exists(scores_path) && exists(score_offsets_path))
            {
                scores_ = std::make_optional<std::vector<char>>();
                score_offsets_ = std::make_optional<std::vector<char>>();
                io::load_data(scores_path, *scores_);
                io::load_data(score_offsets_path, *score_offsets_);
            }
        }
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
        if (!scores_.has_value()) { return std::nullopt; }
        return make_memory_view(scores_.value().data(), scores_.value().size());
    }

    std::optional<memory_view> score_offset_source() const
    {
        if (!score_offsets_.has_value()) { return std::nullopt; }
        return make_memory_view(
            score_offsets_.value().data(), score_offsets_.value().size());
    }

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
    std::optional<std::vector<char>> scores_;
    std::optional<std::vector<char>> score_offsets_;
};

class inverted_index_mapped_data_source {

public:
    explicit inverted_index_mapped_data_source(
        path dir, std::optional<std::string> score_name = std::nullopt)
        : dir_(dir)
    {
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

        documents_.open(index::doc_ids_path(dir));
        counts_.open(index::doc_counts_path(dir));
        document_offsets_.open(index::doc_ids_off_path(dir));
        count_offsets_.open(index::doc_counts_off_path(dir));
        term_collection_frequencies_.open(index::term_doc_freq_path(dir));
        term_map_.open(index::term_map_path(dir));
        title_map_.open(index::title_map_path(dir));
        document_sizes_.open(index::doc_sizes_path(dir));
        term_collection_occurrences_.open(index::term_occurrences_path(dir));
        properties_.open(index::properties_path(dir));

        if (score_name.has_value())
        {
            auto scores_path = dir / (*score_name + ".scores");
            auto score_offsets_path = dir / (*score_name + ".offsets");
            if (exists(scores_path) && exists(score_offsets_path))
            {
                scores_ = std::make_optional<mapped_file_source>(scores_path);
                score_offsets_ = std::make_optional<mapped_file_source>(
                    score_offsets_path);
            }
        }
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
        if (!scores_.has_value()) { return std::nullopt; }
        return make_memory_view(scores_.value().data(), scores_.value().size());
    }

    std::optional<memory_view> score_offset_source() const
    {
        if (!score_offsets_.has_value()) { return std::nullopt; }
        return make_memory_view(
            score_offsets_.value().data(), score_offsets_.value().size());
    }

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
    std::optional<mapped_file_source> scores_;
    std::optional<mapped_file_source> score_offsets_;
};

};  // namespace irk
