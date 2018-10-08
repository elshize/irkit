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

#include <boost/iterator/permutation_iterator.hpp>
#include <boost/range/iterator_range.hpp>
#include <spdlog/spdlog.h>
#include <cppitertools/itertools.hpp>

#include <irkit/assert.hpp>
#include <irkit/index.hpp>
#include <irkit/index/block_inverted_list.hpp>
#include <irkit/index/source.hpp>
#include <irkit/index/types.hpp>

namespace irk::partition {

using boost::filesystem::path;
using index::document_t;
using index::frequency_t;
using index::term_id_t;

using score_type = inverted_index_view::score_type;
using document_builder_type =
    index::block_list_builder<document_t, stream_vbyte_codec<document_t>>;
using frequency_builder_type =
    index::block_list_builder<frequency_t, stream_vbyte_codec<frequency_t>>;
using score_builder_type =
    index::block_list_builder<score_type, stream_vbyte_codec<score_type>>;

/// Resolves output paths to all shards.
vmap<ShardId, path> resolve_paths(const path& output_dir, int shard_count)
{
    vmap<ShardId, path> paths;
    for (auto shard : ShardId::range(shard_count)) {
        paths.push_back(
            output_dir / fmt::format("{:03d}", static_cast<int32_t>(shard)));
    }
    return paths;
}

/// Partitions document sizes.
auto sizes(
    const path& input_dir,
    const vmap<ShardId, path>& output_dirs,
    const vmap<document_t, ShardId>& shard_mapping)
{
    using size_type = inverted_index_view::size_type;
    auto size_table =
        irk::load_compact_table<size_type>(index::doc_sizes_path(input_dir));
    vmap<ShardId, std::vector<size_type>> shard_sizes(output_dirs.size());
    vmap<ShardId, size_type> avg_shard_sizes(output_dirs.size());
    document_t id(0);
    for (const auto& size : size_table) {
        shard_sizes[shard_mapping[id]].push_back(size);
        avg_shard_sizes[shard_mapping[id]] += size;
        id += 1;
    }

    for (ShardId shard : ShardId::range(output_dirs.size())) {
        std::ofstream os(index::doc_sizes_path(output_dirs[shard]).string());
        irk::build_compact_table(shard_sizes[shard]).serialize(os);
        avg_shard_sizes[shard] /= shard_sizes[shard].size();
    }
    return avg_shard_sizes;
}

/// Partitions document titles and title map.
auto titles(
    const path& input_dir,
    const vmap<ShardId, path>& output_dirs,
    const vmap<document_t, ShardId>& shard_mapping)
{
    auto titles =
        load_lexicon(make_memory_view(index::title_map_path(input_dir)));
    auto keys_per_block = titles.keys_per_block();
    vmap<ShardId, std::vector<std::string>> ShardIditles(
        output_dirs.size());
    document_t id(0);
    for (auto title : titles) {
        ShardIditles[shard_mapping[id]].push_back(std::move(title));
        id += 1;
    }
    for (ShardId shard : ShardId::range(output_dirs.size())) {
        std::ofstream los(index::title_map_path(output_dirs[shard]).string());
        std::ofstream tos(index::titles_path(output_dirs[shard]).string());
        irk::build_lexicon(ShardIditles[shard], keys_per_block).serialize(los);
        for (const auto& title : ShardIditles[shard]) {
            tos << title << '\n';
        }
    }
}

std::vector<document_t> compute_document_mapping(
    const vmap<document_t, ShardId>& shard_mapping, int shard_count)
{
    vmap<ShardId, document_t> next_id(shard_count, 0);
    std::vector<document_t> document_mapping(shard_mapping.size());
    for (auto&& [idx, shard] : iter::enumerate(shard_mapping)) {
        document_mapping[idx] = next_id[shard]++;
    }
    return document_mapping;
}

template<typename Builder, typename DocumentList>
vmap<ShardId, Builder> build_document_lists(
    const DocumentList& documents,
    const vmap<int, ShardId>& shard_mapping,
    const std::vector<document_t>& document_mapping)
{
    vmap<ShardId, Builder> builders(
        shard_mapping.size(), Builder(documents.block_size()));
    for (auto&& id : documents) {
        auto shard = shard_mapping[id];
        builders[shard].add(document_mapping[id]);
    }
    return builders;
}

template<typename Builder, typename PostingList>
vmap<ShardId, Builder> build_payload_lists(
    const PostingList& postings, const vmap<int, ShardId>& shard_mapping)
{
    vmap<ShardId, Builder> builders(
        shard_mapping.size(), Builder(postings.block_size()));
    for (auto&& posting : postings) {
        auto id = posting.document();
        auto shard = shard_mapping[id];
        builders[shard].add(posting.payload());
    }
    return builders;
}

template<typename Builder, typename Index>
vmap<ShardId, std::vector<Builder>> build_score_lists(
    const Index& index,
    term_id_t term_id,
    const vmap<int, ShardId>& shard_mapping,
    const std::vector<std::string>& score_names)
{
    vmap<ShardId, std::vector<Builder>> builders(
        shard_mapping.size(),
        std::vector<Builder>(
            score_names.size(), Builder(index.documents(0).block_size())));
    for (const auto& [idx, name] : iter::enumerate(score_names)) {
        auto postings = index.scored_postings(term_id, name);
        for (auto&& posting : postings) {
            auto id = posting.document();
            auto shard = shard_mapping[id];
            builders[shard][idx].add(posting.payload());
        }
    }
    return builders;
}

template<typename Index, typename BatchRange>
inline void postings_batch(
    const Index& index,
    const vmap<ShardId, path>& output_dirs,
    const BatchRange& term_batch,
    const vmap<document_t, ShardId>& shard_mapping,
    const std::vector<document_t>& document_mapping,
    vmap<ShardId, index::posting_vectors>& vectors,
    const std::vector<std::string>& score_names,
    std::ios_base::openmode mode)
{
    auto log = spdlog::get("partition");
    std::vector<vmap<ShardId, document_builder_type>> document_builders;
    std::vector<vmap<ShardId, frequency_builder_type>> frequency_builders;
    std::vector<vmap<ShardId, std::vector<score_builder_type>>>
        score_builders(score_names.size());
    for (const auto& term_id : term_batch) {
        if (log) { log->info("Processing term {}", term_id); }
        document_builders.push_back(build_document_lists<document_builder_type>(
            index.documents(term_id), shard_mapping, document_mapping));
        frequency_builders.push_back(
            build_payload_lists<frequency_builder_type>(
                index.postings(term_id), shard_mapping));
        score_builders.push_back(build_score_lists<score_builder_type>(
            index, term_id, shard_mapping, score_names));
    }
    for (const auto& [shard, shard_dir] :
         iter::zip(ShardId::range(output_dirs.size()), output_dirs)) {
        index::posting_streams<std::ofstream> out(
            shard_dir, index.score_names(), vectors[shard], mode);
        if (log) {
            log->info("Writing shard {}", shard.as_int());
        }
        for (auto&& [idx, term_id] : iter::enumerate(term_batch)) {
            if (log) { log->info("- Term {}", term_id); }
            auto& document_builder =
                document_builders[term_id - term_batch.front()][shard];
            if (document_builder.size() > 0u) {
                out.write(
                    term_id,
                    document_builder,
                    frequency_builders[term_id - term_batch.front()][shard],
                    score_builders[term_id - term_batch.front()][shard]);
            }
        }
    }
}

inline auto postings(
    const path& input_dir,
    const vmap<ShardId, path>& output_dirs,
    const vmap<document_t, ShardId>& shard_mapping,
    size_t terms_in_batch)
{
    auto log = spdlog::get("partition");
    auto shard_count = output_dirs.size();
    auto document_mapping =
        compute_document_mapping(shard_mapping, shard_count);
    auto source = inverted_index_disk_data_source::from(input_dir).value();
    inverted_index_view index(&source);
    vmap<ShardId, index::posting_vectors> vectors(
        output_dirs.size(), index::posting_vectors(index.score_names()));
    auto nterms = index.term_count();
    auto batches = iter::chunked(iter::range(nterms), terms_in_batch);
    auto nbatches = (nterms + terms_in_batch) / terms_in_batch;
    if (log) { log->info("{}", index.term_count()); }
    auto first_batch = *batches.begin();
    postings_batch(
        index,
        output_dirs,
        std::vector<size_t>(first_batch.begin(), first_batch.end()),
        shard_mapping,
        document_mapping,
        vectors,
        index.score_names(),
        std::ios_base::binary);
    for (auto&& term_batch : iter::slice(batches, size_t(1), nbatches)) {
        postings_batch(
            index,
            output_dirs,
            std::vector<size_t>(term_batch.begin(), term_batch.end()),
            shard_mapping,
            document_mapping,
            vectors,
            index.score_names(),
            std::ios_base::ate);
    }
    vmap<ShardId, frequency_t> total_occurrences;
    for (auto&& [shard, shard_vectors] : vectors.entries()) {
        total_occurrences.push_back(shard_vectors.total_occurrences);
        shard_vectors.write(
            input_dir,
            output_dirs[shard],
            index.terms().keys_per_block(),
            index.score_names());
    }
    return total_occurrences;
}

inline void write_properties(
    const path& input_dir,
    const vmap<ShardId, path>& output_dirs,
    const vmap<ShardId, size_t>& document_counts,
    const vmap<ShardId, inverted_index_view::size_type>& avg_document_sizes,
    const vmap<ShardId, frequency_t>& total_occurrences)
{
    std::ifstream prop_stream(index::properties_path(input_dir).string());
    nlohmann::json properties;
    prop_stream >> properties;
    size_t skip_block_size = properties["skip_block_size"];
    for (auto&& [idx, dir] : iter::enumerate(output_dirs)) {
        ShardId shard(idx);
        std::ofstream os(index::properties_path(dir).string());
        nlohmann::json j = {{"documents", document_counts[shard]},
                            {"occurrences", total_occurrences[shard]},
                            {"skip_block_size", skip_block_size},
                            {"avg_document_size", avg_document_sizes[shard]}};
        os << std::setw(4) << j << std::endl;
    }
}

void index(
    const path& input_dir,
    const path& output_dir,
    const irk::vmap<document_t, ShardId>& shard_mapping,
    size_t terms_in_batch,
    size_t shard_count)
{
    auto shard_paths = irk::partition::resolve_paths(output_dir, shard_count);
    for (const auto& path : shard_paths) {
        boost::filesystem::create_directory(path);
    }
    auto avg_sizes =
        irk::partition::sizes(input_dir, shard_paths, shard_mapping);
    irk::partition::titles(input_dir, shard_paths, shard_mapping);
    auto total_occurrences = irk::partition::postings(
        input_dir, shard_paths, shard_mapping, terms_in_batch);
    vmap<ShardId, size_t> document_counts(shard_count, 0);
    for (const auto& shard : shard_mapping) {
        document_counts[shard] += 1;
    }
    write_properties(
        input_dir,
        shard_paths,
        document_counts,
        avg_sizes,
        total_occurrences);
}

};  // namespace irk::partition
