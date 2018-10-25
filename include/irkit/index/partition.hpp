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

#include <irkit/algorithm/transform.hpp>
#include <irkit/assert.hpp>
#include <irkit/index.hpp>
#include <irkit/index/block_inverted_list.hpp>
#include <irkit/index/source.hpp>
#include <irkit/index/types.hpp>

namespace irk {

using boost::filesystem::filesystem_error;
using boost::filesystem::path;
using index::document_t;
using index::frequency_t;
using index::term_id_t;
using iter::enumerate;
using iter::zip;

namespace index {

    /// A writer for posting-related vectors, such as offsets, max scores,
    /// collection term frequencies, etc.
    struct posting_vectors {
        std::vector<term_id_t> term_ids;
        std::vector<offset_t> document_offsets;
        std::vector<offset_t> frequency_offsets;
        std::vector<std::vector<offset_t>> score_offsets;
        std::vector<std::vector<inverted_index_view::score_type>> max_scores;
        std::vector<std::vector<inverted_index_view::score_type>> exp_scores;
        std::vector<std::vector<inverted_index_view::score_type>> var_scores;
        std::vector<frequency_t> term_frequencies;
        std::vector<frequency_t> term_occurrences;

        frequency_t total_occurrences = 0;

        offset_t cur_document_offset = 0;
        offset_t cur_frequency_offset = 0;
        std::vector<offset_t> cur_score_offsets;
        std::vector<std::string> score_names;

        posting_vectors(std::vector<std::string> score_names = {})
            : score_offsets(score_names.size()),
              max_scores(score_names.size()),
              exp_scores(score_names.size()),
              var_scores(score_names.size()),
              cur_score_offsets(score_names.size(), 0),
              score_names(std::move(score_names))
        {}

        template<typename Range>
        void write(
            const boost::filesystem::path& input_dir,
            const boost::filesystem::path& output_dir,
            int lex_keys_per_block,
            const Range& score_names)
        {
            write_terms(input_dir, output_dir, lex_keys_per_block);
            build_offset_table(document_offsets)
                .serialize(doc_ids_off_path(output_dir));
            build_offset_table(frequency_offsets)
                .serialize(doc_counts_off_path(output_dir));
            for (const auto& [idx, name] : iter::enumerate(score_names)) {
                auto paths = index::score_paths(output_dir, name);
                build_offset_table(score_offsets[idx]).serialize(paths.offsets);
                using score_type = inverted_index_view::score_type;
                build_compact_table<score_type>(max_scores[idx])
                    .serialize(paths.max_scores);
                build_compact_table<score_type>(exp_scores[idx])
                    .serialize(paths.exp_values);
                build_compact_table<score_type>(var_scores[idx])
                    .serialize(paths.variances);
            }
            build_compact_table<frequency_t>(term_frequencies)
                .serialize(term_doc_freq_path(output_dir));
            build_compact_table<frequency_t>(term_occurrences)
                .serialize(term_occurrences_path(output_dir));
        }

        /// Accumulates data for a term.
        ///
        /// This function needs **not** to be called with consecutive term IDs,
        /// but the IDs must be in increasing order.
        void push(
            term_id_t term_id,
            offset_t document_size,
            offset_t frequency_size,
            std::vector<offset_t> score_sizes,
            std::vector<inverted_index_view::score_type> max_score_vec,
            frequency_t frequency,
            frequency_t occurrences)
        {
            term_ids.push_back(term_id);
            document_offsets.push_back(cur_document_offset);
            frequency_offsets.push_back(cur_frequency_offset);
            cur_document_offset += document_size;
            cur_frequency_offset += frequency_size;
            for (auto&& [idx, size] : iter::enumerate(score_sizes)) {
                score_offsets[idx].push_back(cur_score_offsets[idx]);
                cur_score_offsets[idx] += size;
            }
            term_frequencies.push_back(frequency);
            term_occurrences.push_back(occurrences);
            for (auto&& [max, mscores] : zip(max_score_vec, max_scores)) {
                mscores.push_back(max);
            }
            total_occurrences += occurrences;
        }

    private:
        void write_terms(
            const boost::filesystem::path& input_dir,
            const boost::filesystem::path& output_dir,
            int keys_per_block)
        {
            io::filter_lines(
                terms_path(input_dir), terms_path(output_dir), term_ids);
            build_lexicon(terms_path(output_dir), keys_per_block)
                .serialize(term_map_path(output_dir));
            build_compact_table(term_frequencies)
                .serialize(term_doc_freq_path(output_dir));
        }
    };

    /// All streams for posting-like data.
    template<typename Stream>
    struct posting_streams {
        Stream documents;
        Stream frequencies;
        std::vector<Stream> scores;
        posting_vectors& vectors;

        posting_streams(
            const boost::filesystem::path& dir,
            const std::vector<std::string>& score_names,
            posting_vectors& vectors,
            std::ios_base::openmode mode = std::ios_base::binary)
            : documents(doc_ids_path(dir).string(), mode),
              frequencies(doc_counts_path(dir).string(), mode),
              vectors(vectors)
        {
            for (const std::string& score : score_names) {
                auto score_path = dir / fmt::format("{}.scores", score);
                auto score_offsets_path =
                    dir / fmt::format("{}.offsets", score);
                scores.emplace_back(score_path.string(), mode);
            }
        }

        /// Writes out the builders to files.
        template<
            typename DocBuilder,
            typename FreqBuilder,
            typename ScoreBuilder>
        void write(
            term_id_t term_id,
            DocBuilder& document_builder,
            FreqBuilder& frequency_builder,
            std::vector<ScoreBuilder>& score_builders)
        {
            using score_type = inverted_index_view::score_type;
            EXPECTS(score_builders.size() == scores.size());
            const auto& frequency_vector = frequency_builder.values();
            frequency_t occurrences = std::accumulate(
                frequency_vector.begin(),
                frequency_vector.end(),
                frequency_t(0),
                std::plus<frequency_t>());
            std::vector<score_type> max_scores;
            std::vector<offset_t> score_sizes;
            for (auto&& [builder, stream] : iter::zip(score_builders, scores)) {
                auto max = *std::max_element(
                    builder.values().begin(), builder.values().end());
                max_scores.push_back(max);
                score_sizes.push_back(builder.write(stream));
            }
            vectors.push(
                term_id,
                document_builder.write(documents),
                frequency_builder.write(frequencies),
                score_sizes,
                max_scores,
                document_builder.size(),
                occurrences);
        }
    };

}

namespace detail::partition {

    /// Resolves output paths to all shards.
    vmap<ShardId, path> resolve_paths(const path& output_dir, int shard_count)
    {
        vmap<ShardId, path> paths(shard_count);
        std::generate_n(
            paths.begin(), shard_count, [&output_dir, shard = 0]() mutable {
                return output_dir / fmt::format("{:03d}", shard++);
            });
        return paths;
    }

    /// Computes ID mapping from global to local document ID.
    static std::vector<document_t> compute_document_mapping(
        const vmap<document_t, ShardId>& shard_mapping, const int shard_count)
    {
        vmap<ShardId, document_t> next_id(shard_count, 0);
        std::vector<document_t> document_mapping(shard_mapping.size());
        for (auto&& [document, shard] : zip(document_mapping, shard_mapping)) {
            document = next_id[shard]++;
        }
        return document_mapping;
    }

    /// A convenient wrapper for a set of functions for index partitioning.
    class Partition {
    public:
        using score_type = inverted_index_view::score_type;
        using document_builder_type = index::block_list_builder<
            document_t,
            stream_vbyte_codec<document_t>,
            true>;
        using frequency_builder_type = index::
            block_list_builder<frequency_t, stream_vbyte_codec<frequency_t>>;
        using score_builder_type = index::
            block_list_builder<score_type, stream_vbyte_codec<score_type>>;

        Partition(
            int32_t shard_count,
            int32_t document_count,
            const path& input_dir,
            const vmap<ShardId, path>& shard_dirs,
            const vmap<document_t, ShardId>& shard_mapping,
            const std::vector<document_t>& document_mapping)
            : shard_count_(shard_count),
              document_count_(document_count),
              input_dir_(input_dir),
              shard_dirs_(shard_dirs),
              shard_mapping_(shard_mapping),
              document_mapping_(document_mapping)
        {}

        /// Partitions document sizes.
        auto sizes()
        {
            using size_type = inverted_index_view::size_type;
            auto size_table = irk::load_compact_table<size_type>(
                index::doc_sizes_path(input_dir_));
            vmap<ShardId, std::vector<size_type>> shard_sizes(shard_count_);
            vmap<ShardId, size_t> avg_shard_sizes(shard_count_, 0);

            for (const auto& [shard_assignment, size] :
                 zip(shard_mapping_, size_table))
            {
                shard_sizes[shard_assignment].push_back(size);
                avg_shard_sizes[shard_assignment] += size;
            }

            for (auto&& [dir, sizes, avg_size] :
                 zip(shard_dirs_, shard_sizes, avg_shard_sizes))
            {
                if (not exists(dir)) {
                    create_directory(dir);
                }
                std::ofstream os(index::doc_sizes_path(dir).string());
                irk::build_compact_table(sizes).serialize(os);
                avg_size /= sizes.size();
            }

            return avg_shard_sizes;
        }

        /// Partitions document titles and title map.
        auto titles()
        {
            auto [buf, lex_view] = make_memory_view(index::title_map_path(input_dir_));
            (void)buf;
            auto titles = load_lexicon(lex_view);
            auto keys_per_block = titles.keys_per_block();
            vmap<ShardId, std::vector<std::string>> shard_titles(shard_count_);
            for (const auto& [shard, title] : zip(shard_mapping_, titles)) {
                shard_titles[shard].push_back(title);
            }
            for (const auto& [shard_dir, partitioned_titles] :
                 zip(shard_dirs_, shard_titles))
            {
                if (not exists(shard_dir)) {
                    create_directory(shard_dir);
                }
                std::ofstream los(index::title_map_path(shard_dir).string());
                std::ofstream tos(index::titles_path(shard_dir).string());
                irk::build_lexicon(partitioned_titles, keys_per_block)
                    .serialize(los);
                for (const auto& title : partitioned_titles) {
                    tos << title << '\n';
                }
            }
        }

        template<typename DocumentList>
        document_builder_type
        filter_document_lists(const DocumentList& documents, ShardId shard)
        {
            const auto& block_size = documents.block_size();
            document_builder_type builder(block_size);
            for (auto&& id : documents) {
                if (shard != shard_mapping_[id]) {
                    continue;
                }
                auto local_doc_id = document_mapping_[id];
                builder.add(local_doc_id);
            }
            return builder;
        }

        template<typename PostingList>
        frequency_builder_type
        filter_freq_lists(const PostingList& postings, ShardId shard)
        {
            const auto& block_size = postings.block_size();
            frequency_builder_type builder(block_size);
            for (auto&& posting : postings) {
                if (shard != shard_mapping_[posting.document()]) {
                    continue;
                }
                builder.add(posting.payload());
            }
            return builder;
        }

        template<typename Index>
        std::vector<score_builder_type> filter_score_lists(
            const Index& index,
            term_id_t term_id,
            const std::vector<std::string>& score_names,
            ShardId shard)
        {
            size_t block_size = index.documents(0).block_size();
            std::vector<score_builder_type> builders;
            for (const auto& _ : score_names) {
                (void)_;
                builders.emplace_back(block_size);
            }
            for (const auto& [idx, name] : iter::enumerate(score_names)) {
                auto postings = index.scored_postings(term_id, name);
                for (auto&& posting : postings) {
                    if (shard != shard_mapping_[posting.document()]) {
                        continue;
                    }
                    builders[idx].add(posting.payload());
                }
            }
            return builders;
        }

        template<typename DocumentList>
        vmap<ShardId, document_builder_type>
        build_document_lists(const DocumentList& documents)
        {
            const auto& block_size = documents.block_size();
            vmap<ShardId, document_builder_type> builders(
                shard_count_, document_builder_type(block_size));
            for (auto&& id : documents) {
                auto shard = shard_mapping_[id];
                auto local_doc_id = document_mapping_[id];
                builders[shard].add(local_doc_id);
            }
            return builders;
        }

        template<typename PostingList>
        vmap<ShardId, frequency_builder_type>
        build_payload_lists(const PostingList& postings)
        {
            const auto& block_size = postings.block_size();
            vmap<ShardId, frequency_builder_type> builders(
                shard_count_, frequency_builder_type(block_size));
            for (auto&& posting : postings) {
                auto shard = shard_mapping_[posting.document()];
                builders[shard].add(posting.payload());
            }
            return builders;
        }

        template<typename Index>
        vmap<ShardId, std::vector<score_builder_type>> build_score_lists(
            const Index& index,
            term_id_t term_id,
            const std::vector<std::string>& score_names)
        {
            size_t block_size = index.documents(0).block_size();
            vmap<ShardId, std::vector<score_builder_type>> builders(
                document_count_);
            for (auto& builder : builders) {
                for (const auto& _ : score_names) {
                    (void)_;
                    builder.emplace_back(block_size);
                }
            }
            for (const auto& [idx, name] : iter::enumerate(score_names)) {
                auto postings = index.scored_postings(term_id, name);
                for (auto&& posting : postings) {
                    auto id = posting.document();
                    auto shard = shard_mapping_[id];
                    builders[shard][idx].add(posting.payload());
                }
            }
            return builders;
        }

        /// Partitions all posting-like data.
        inline auto postings_once()
        {
            auto log = spdlog::get("partition");

            vmap<ShardId, frequency_t> total_occurrences;
            auto source = inverted_index_mapped_data_source::from(
                              input_dir_, index::all_score_names(input_dir_))
                              .value();
            inverted_index_view index(&source);

            vmap<ShardId, index::posting_vectors> vectors(
                shard_count_, index::posting_vectors(index.score_names()));
            vmap<ShardId, index::posting_streams<std::ofstream>> outputs;
            for (ShardId shard : ShardId::range(shard_dirs_.size())) {
                outputs.emplace_back(
                    shard_dirs_[shard], index.score_names(), vectors[shard]);
            }
            for (auto term_id : iter::range(index.term_count())) {
                auto document_builders =
                    build_document_lists(index.documents(term_id));
                auto frequency_builders =
                    build_payload_lists(index.postings(term_id));
                auto score_builders =
                    build_score_lists(index, term_id, index.score_names());
                for (ShardId shard : ShardId::range(shard_count_)) {
                    auto& document_builder = document_builders[shard];
                    if (document_builder.size() > 0u) {
                        outputs[shard].write(
                            term_id,
                            document_builder,
                            frequency_builders[shard],
                            score_builders[shard]);
                    }
                }
            }
            for (ShardId shard : ShardId::range(shard_count_)) {
                total_occurrences.push_back(vectors[shard].total_occurrences);
                vectors[shard].write(
                    input_dir_,
                    shard_dirs_[shard],
                    index.terms().keys_per_block(),
                    index.score_names());
            }
            return total_occurrences;
        }

        /// Partitions all posting-like data.
        inline auto postings(size_t terms_in_batch)
        {
            auto log = spdlog::get("partition");
            auto source = inverted_index_mapped_data_source::from(
                              input_dir_, index::all_score_names(input_dir_))
                              .value();
            inverted_index_view index(&source);
            vmap<ShardId, frequency_t> total_occurrences;
            for (const auto& [shard, shard_dir] :
                 iter::zip(ShardId::range(shard_count_), shard_dirs_))
            {
                if (log) {
                    log->info(
                        "Partitioning postings for shard {}",
                        static_cast<size_t>(shard));
                }
                index::posting_vectors vectors(index.score_names());
                index::posting_streams<std::ofstream> out(
                    shard_dir,
                    index.score_names(),
                    vectors,
                    std::ios_base::binary);
                for (auto term_id : iter::range(index.term_count())) {
                    auto document_builder =
                        filter_document_lists(index.documents(term_id), shard);
                    if (document_builder.size() > 0u) {
                        auto freq_builder =
                            filter_freq_lists(index.postings(term_id), shard);
                        auto score_builders = filter_score_lists(
                            index, term_id, index.score_names(), shard);
                        out.write(
                            term_id,
                            document_builder,
                            freq_builder,
                            score_builders);
                    }
                }
                total_occurrences.push_back(vectors.total_occurrences);
                vectors.write(
                    input_dir_,
                    shard_dirs_[shard],
                    index.terms().keys_per_block(),
                    index.score_names());
            }
            return total_occurrences;
        }

        /// Partitions all posting-like data.
        inline auto postings_(size_t terms_in_batch)
        {
            auto log = spdlog::get("partition");
            auto source = inverted_index_mapped_data_source::from(
                              input_dir_, index::all_score_names(input_dir_))
                              .value();
            inverted_index_view index(&source);
            vmap<ShardId, index::posting_vectors> vectors(
                shard_count_, index::posting_vectors(index.score_names()));
            auto nterms = index.term_count();
            auto batches = iter::chunked(iter::range(nterms), terms_in_batch);
            auto nbatches = (nterms + terms_in_batch) / terms_in_batch;
            auto first_batch = *batches.begin();
            int batch(0);
            if (log) { log->info("Batch {}/{}", batch++, nbatches); }
            postings_batch(
                index,
                std::vector<size_t>(first_batch.begin(), first_batch.end()),
                vectors,
                index.score_names(),
                std::ios_base::binary);
            for (auto&& term_batch :
                 iter::slice(batches, size_t(1), nbatches)) {
                if (log) { log->info("Batch {}/{}", batch++, nbatches); }
                postings_batch(
                    index,
                    std::vector<size_t>(term_batch.begin(), term_batch.end()),
                    vectors,
                    index.score_names(),
                    std::ios_base::app);
            }
            vmap<ShardId, frequency_t> total_occurrences;
            if (log) { log->info("Writing..."); }
            for (auto&& [shard, shard_vectors] : vectors.entries()) {
                total_occurrences.push_back(shard_vectors.total_occurrences);
                shard_vectors.write(
                    input_dir_,
                    shard_dirs_[shard],
                    index.terms().keys_per_block(),
                    index.score_names());
            }
            return total_occurrences;
        }

        /// Partitions the entire index.
        nonstd::expected<void, std::string> index(size_t terms_in_batch)
        {
            try {
                for (const auto& path : shard_dirs_) {
                    boost::filesystem::create_directory(path);
                }
                titles();
                auto avg_sizes = sizes();
                //auto total_occurrences = postings(terms_in_batch);
                auto total_occurrences = postings_once();
                vmap<ShardId, size_t> document_counts(shard_count_, 0);
                for (const auto& shard : shard_mapping_) {
                    document_counts[shard] += 1;
                }
                write_properties(document_counts, avg_sizes, total_occurrences);
                return nonstd::expected<void, std::string>();
            } catch (boost::filesystem::filesystem_error& error) {
                return nonstd::make_unexpected(std::string(error.what()));
            }
        }

        /// Partitions posting-like data for a range of terms.
        template<typename Index, typename BatchRange>
        inline void postings_batch(
            const Index& index,
            const BatchRange& term_batch,
            vmap<ShardId, index::posting_vectors>& vectors,
            const std::vector<std::string>& score_names,
            std::ios_base::openmode mode)
        {
            auto log = spdlog::get("partition");
            if (log) {
                log->info(
                    "Processing terms [{}, {}]",
                    term_batch.front(),
                    term_batch.back());
                log->info("Building...");
            }
            std::vector<vmap<ShardId, document_builder_type>> document_builders;
            std::vector<vmap<ShardId, frequency_builder_type>>
                frequency_builders;
            std::vector<vmap<ShardId, std::vector<score_builder_type>>>
                score_builders;
            for (const auto& term_id : term_batch) {
                document_builders.push_back(
                    build_document_lists(index.documents(term_id)));
                frequency_builders.push_back(
                    build_payload_lists(index.postings(term_id)));
                score_builders.push_back(
                    build_score_lists(index, term_id, score_names));
            }
            if (log) { log->info("Writing..."); }
            for (const auto& [shard, shard_dir] :
                 iter::zip(ShardId::range(shard_count_), shard_dirs_))
            {
                index::posting_streams<std::ofstream> out(
                    shard_dir, index.score_names(), vectors[shard], mode);
                for (auto&& [idx, term_id] : iter::enumerate(term_batch)) {
                    auto& document_builder = document_builders[idx][shard];
                    if (document_builder.size() > 0u) {
                        out.write(
                            term_id,
                            document_builder,
                            frequency_builders[idx][shard],
                            score_builders[idx][shard]);
                    }
                }
            }
        }

        inline void write_properties(
            const vmap<ShardId, size_t>& document_counts,
            const vmap<ShardId, size_t>& avg_document_sizes,
            const vmap<ShardId, frequency_t>& total_occurrences)
        {
            std::ifstream prop_stream(
                index::properties_path(input_dir_).string());
            nlohmann::json properties;
            prop_stream >> properties;
            size_t skip_block_size = properties["skip_block_size"];
            for (auto&& [idx, dir] : enumerate(shard_dirs_)) {
                ShardId shard(idx);
                std::ofstream os(index::properties_path(dir).string());
                nlohmann::json j = {
                    {"documents", document_counts[shard]},
                    {"occurrences", total_occurrences[shard]},
                    {"skip_block_size", skip_block_size},
                    {"avg_document_size", avg_document_sizes[shard]}};
                os << std::setw(4) << j << std::endl;
            }
        }

        const int32_t shard_count_;
        const int32_t document_count_;
        const path& input_dir_;
        const vmap<ShardId, path>& shard_dirs_;
        const vmap<document_t, ShardId>& shard_mapping_;
        const std::vector<document_t>& document_mapping_;
    };

}  // namespace detail::partition

/// Partitions an inverted index in `input_path` into `shard_count` shards
/// described by `shard_mapping`, writing them to `output_dir`.
///
/// \param input_dir        an index directory
/// \param output_dir       a directory to write the resulting shards to
/// \param shard_mapping    `i`-th value is the shard assigned to document `i`
/// \param shard_count      total number of shards
/// \param terms_in_batch   how many terms at a time to process in memory
///                         before flushing to disk
nonstd::expected<void, std::string> partition_index(
    const path& input_dir,
    const path& output_dir,
    const vmap<document_t, ShardId>& shard_mapping,
    int shard_count,
    int terms_in_batch)
{
    int32_t document_count = shard_mapping.size();
    auto shard_dirs = detail::partition::resolve_paths(output_dir, shard_count);
    auto document_mapping =
        detail::partition::compute_document_mapping(shard_mapping, shard_count);
    auto log = spdlog::get("partition");
    if (log) {
        log->info(
            "Partitioning index {} into {} shards in {} [batch size = {}]",
            input_dir.string(),
            shard_count,
            output_dir.string(),
            terms_in_batch);
    }
    auto partition = detail::partition::Partition(
        shard_count,
        document_count,
        input_dir,
        shard_dirs,
        shard_mapping,
        document_mapping);
    return partition.index(terms_in_batch);
}

}  // namespace irk
