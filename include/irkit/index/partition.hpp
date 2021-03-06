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
#include <numeric>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <boost/iterator/permutation_iterator.hpp>
#include <boost/range/iterator_range.hpp>
#include <cppitertools/itertools.hpp>
#include <pstl/algorithm>
#include <pstl/execution>
#include <pstl/numeric>
#include <spdlog/spdlog.h>

#include <irkit/algorithm/transform.hpp>
#include <irkit/assert.hpp>
#include <irkit/index.hpp>
#include <irkit/index/source.hpp>
#include <irkit/index/types.hpp>
#include <irkit/list/standard_block_list.hpp>

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
            std::vector<inverted_index_view::score_type> score_exp_vec,
            std::vector<inverted_index_view::score_type> score_var_vec,
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
            for (auto&& [max, vec] : zip(max_score_vec, max_scores)) {
                vec.push_back(max);
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
            namespace a = boost::accumulators;
            using accumulator = a::accumulator_set<
                score_type,
                a::stats<a::tag::mean, a::tag::variance, a::tag::max>>;
            EXPECTS(score_builders.size() == scores.size());
            const auto& frequency_vector = frequency_builder.values();
            frequency_t occurrences = std::accumulate(
                frequency_vector.begin(),
                frequency_vector.end(),
                frequency_t(0),
                std::plus<frequency_t>());
            std::vector<score_type> max_scores, exps, vars;
            std::vector<offset_t> score_sizes;
            for (auto&& [builder, stream] : iter::zip(score_builders, scores)) {
                accumulator acc;
                std::for_each(
                    builder.values().begin(),
                    builder.values().end(),
                    [&acc](auto score) { acc(score); });
                max_scores.push_back(a::max(acc));
                exps.push_back(a::mean(acc));
                vars.push_back(a::variance(acc));
                score_sizes.push_back(builder.write(stream));
            }
            vectors.push(
                term_id,
                document_builder.write(documents),
                frequency_builder.write(frequencies),
                score_sizes,
                max_scores,
                exps,
                vars,
                document_builder.size(),
                occurrences);
        }
    };

}

namespace detail::partition {

    /// Resolves output paths to all shards.
    Vector<ShardId, path> resolve_paths(const path& output_dir, int shard_count)
    {
        Vector<ShardId, path> paths(shard_count);
        std::generate_n(
            paths.begin(), shard_count, [&output_dir, shard = 0]() mutable {
                return output_dir / fmt::format("{:03d}", shard++);
            });
        return paths;
    }

    /// Computes ID mapping from global to local document ID.
    static Vector<document_t>
    compute_document_mapping(const Vector<document_t, ShardId>& shard_mapping,
                             const int shard_count)
    {
        Vector<ShardId, document_t> next_id(shard_count, 0);
        Vector<document_t> document_mapping(shard_mapping.size());
        for (auto&& [document, shard] : zip(document_mapping, shard_mapping)) {
            document = next_id[shard]++;
        }
        return document_mapping;
    }

    /// Computes ID mapping from local to global document ID.
    static Vector<ShardId, Vector<document_t>>
    compute_reverse_mapping(const Vector<document_t, ShardId>& shard_mapping, const int shard_count)
    {
        Vector<ShardId, Vector<document_t>> reverse_mapping(shard_count);
        for (auto global_id : iter::range(shard_mapping.size())) {
            auto shard = shard_mapping[global_id];
            reverse_mapping[shard].push_back(global_id);
        }
        return reverse_mapping;
    }

    /// A convenient wrapper for a set of functions for index partitioning.
    class Partition {
    public:
        using score_type = inverted_index_view::score_type;
        using document_builder_type = ir::
            Standard_Block_List_Builder<document_t, stream_vbyte_codec<document_t>, true>;
        using frequency_builder_type = ir::
            Standard_Block_List_Builder<frequency_t, stream_vbyte_codec<frequency_t>, false>;
        using score_builder_type = ir::
            Standard_Block_List_Builder<score_type, stream_vbyte_codec<score_type>, false>;

        Partition(int32_t shard_count,
                  int32_t document_count,
                  const path& input_dir,
                  const Vector<ShardId, path>& shard_dirs,
                  const Vector<document_t, ShardId>& shard_mapping,
                  const Vector<document_t>& document_mapping,
                  const Vector<ShardId, Vector<document_t>>& reverse_mapping)
            : shard_count_(shard_count),
              document_count_(document_count),
              input_dir_(input_dir),
              shard_dirs_(shard_dirs),
              shard_mapping_(shard_mapping),
              document_mapping_(document_mapping),
              reverse_mapping_(reverse_mapping)
        {}

        /// Partitions document sizes.
        auto sizes()
        {
            using size_type = inverted_index_view::size_type;
            auto size_table = irk::load_compact_table<size_type>(
                index::doc_sizes_path(input_dir_));
            Vector<ShardId, std::vector<size_type>> shard_sizes(shard_count_);
            Vector<ShardId, size_t> avg_shard_sizes(shard_count_, 0);

            for (const auto& [shard_assignment, size] :
                 zip(shard_mapping_, size_table))
            {
                shard_sizes[shard_assignment].push_back(size);
                avg_shard_sizes[shard_assignment] += size;
            }

            Vector<ShardId, size_t> max_shard_sizes(shard_count_);
            std::transform(
                std::execution::par_unseq,
                shard_sizes.begin(),
                shard_sizes.end(),
                max_shard_sizes.begin(),
                [](const auto& size_vec) {
                    return *std::max_element(
                        std::execution::unseq,
                        size_vec.begin(),
                        size_vec.end());
                });

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

            return std::make_pair(
                std::move(avg_shard_sizes), std::move(max_shard_sizes));
        }

        /// Partitions document titles and title map.
        auto titles()
        {
            auto [buf, lex_view] =
                make_memory_view(index::title_map_path(input_dir_));
            (void)buf;
            auto titles = load_lexicon(lex_view);
            auto keys_per_block = titles.keys_per_block();
            Vector<ShardId, std::vector<std::string>> shard_titles(shard_count_);
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
        Vector<ShardId, document_builder_type> build_document_lists(const DocumentList& documents)
        {
            const auto& block_size = documents.block_size();
            Vector<ShardId, document_builder_type> builders(shard_count_,
                                                            document_builder_type(block_size));
            for (auto&& id : documents) {
                auto shard = shard_mapping_[id];
                auto local_doc_id = document_mapping_[id];
                builders[shard].add(local_doc_id);
            }
            return builders;
        }

        template<typename PostingList>
        Vector<ShardId, frequency_builder_type> build_payload_lists(const PostingList& postings)
        {
            const auto& block_size = postings.block_size();
            Vector<ShardId, frequency_builder_type> builders(shard_count_,
                                                             frequency_builder_type(block_size));
            for (auto&& posting : postings) {
                auto shard = shard_mapping_[posting.document()];
                builders[shard].add(posting.payload());
            }
            return builders;
        }

        template<typename Index>
        Vector<ShardId, std::vector<score_builder_type>>
        build_score_lists(const Index& index,
                          term_id_t term_id,
                          const std::vector<std::string>& score_names)
        {
            size_t block_size = index.documents(0).block_size();
            Vector<ShardId, std::vector<score_builder_type>> builders(document_count_);
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

        template<typename DocumentList>
        std::vector<document_t> document_vector(const DocumentList& documents)
        {
            return std::vector<document_t>(documents.begin(), documents.end());
        }

        template<typename FreqList>
        std::vector<frequency_t> frequency_vector(const FreqList& frequencies)
        {
            return std::vector<frequency_t>(
                frequencies.begin(), frequencies.end());
        }

        template<typename Index>
        std::vector<std::vector<inverted_index_view::score_type>> score_vectors(
            const Index& index,
            term_id_t term_id,
            const std::vector<std::string>& score_names)
        {
            std::vector<std::vector<inverted_index_view::score_type>>
                score_vectors(score_names.size());
            for (const auto& [idx, name] : iter::enumerate(score_names)) {
                auto scores = index.scores(term_id, name);
                score_vectors[idx] =
                    std::vector<inverted_index_view::score_type>(
                        scores.begin(), scores.end());
            }
            return score_vectors;
        }

        /// Partitions all posting-like data.
        inline auto postings_once()
        {
            auto log = spdlog::get("partition");
            Vector<ShardId, frequency_t> total_occurrences;
            auto source = irtl::value(
                Inverted_Index_Mapped_Source::from(input_dir_, index::all_score_names(input_dir_)));
            inverted_index_view index(source);
            auto score_names = index.score_names();

            Vector<ShardId, index::posting_vectors> vectors(shard_count_,
                                                            index::posting_vectors(score_names));
            Vector<ShardId, index::posting_streams<std::ofstream>> outputs;
            for (ShardId shard : ShardId::range(shard_dirs_.size())) {
                outputs.emplace_back(
                    shard_dirs_[shard], score_names, vectors[shard]);
            }
            auto term_count = index.term_count();
            for (auto term_id : iter::range(term_count)) {
                if (log and term_id % 100000 == 0) {
                    log->info("Partitioning postings for term {}/{}", term_id, term_count);
                }
                auto documents = document_vector(index.documents(term_id));
                auto frequencies = frequency_vector(index.frequencies(term_id));
                auto scores = score_vectors(index, term_id, score_names);

                const auto& block_size = index.skip_block_size();
                Vector<ShardId, document_builder_type> document_builders(
                    shard_count_, document_builder_type(block_size));
                Vector<ShardId, frequency_builder_type> frequency_builders(
                    shard_count_, frequency_builder_type(block_size));
                Vector<ShardId, std::vector<score_builder_type>> score_builders(
                    shard_count_,
                    std::vector<score_builder_type>(score_names.size(),
                                                    score_builder_type(block_size)));
                for (auto idx : iter::range(documents.size())) {
                    auto id = documents[idx];
                    auto shard = shard_mapping_[id];
                    auto local_doc_id = document_mapping_[id];
                    document_builders[shard].add(local_doc_id);
                    frequency_builders[shard].add(frequencies[idx]);
                    for (auto i : iter::range(score_names.size())) {
                        score_builders[shard][i].add(scores[i][idx]);
                    }
                }
                for (ShardId shard : ShardId::range(shard_count_)) {
                    auto& document_builder = document_builders[shard];
                    if (document_builder.size() > 0) {
                        outputs[shard].write(
                            term_id,
                            document_builder,
                            frequency_builders[shard],
                            score_builders[shard]);
                    }
                }
            }
            if (log) {
                log->info("Writing vectors");
            }
            for (ShardId shard : ShardId::range(shard_count_)) {
                total_occurrences.push_back(vectors[shard].total_occurrences);
                vectors[shard].write(
                    input_dir_,
                    shard_dirs_[shard],
                    index.terms().keys_per_block(),
                    score_names);
            }
            return total_occurrences;
        }

        /// Partitions all posting-like data.
        inline auto postings(size_t terms_in_batch)
        {
            auto log = spdlog::get("partition");
            auto source = irtl::value(
                Inverted_Index_Mapped_Source::from(input_dir_, index::all_score_names(input_dir_)));
            inverted_index_view index(source);
            Vector<ShardId, frequency_t> total_occurrences;
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
                    if (document_builder.size() > 0) {
                        auto freq_builder = filter_freq_lists(index.postings(term_id), shard);
                        auto score_builders = filter_score_lists(
                            index, term_id, index.score_names(), shard);
                        out.write(term_id, document_builder, freq_builder, score_builders);
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

        [[nodiscard]] std::vector<std::string> scores_with_stats() const
        {
            auto score_stats = index::find_score_stats_paths(input_dir_);
            std::vector<std::string> names;
            irk::transform_range_if(score_stats,
                                    std::back_inserter(names),
                                    [](auto const& entry) -> std::string { return entry.first; },
                                    [](auto const& entry) -> bool {
                                        auto const& stats = entry.second;
                                        return stats.max or stats.mean or stats.var;
                                    });
            return names;
        }

        /// Partitions the entire index.
        nonstd::expected<void, std::string> index()
        {
            try {
                auto log = spdlog::get("partition");
                if (log) {
                    for (auto const& score_name : scores_with_stats()) {
                        log->warn(
                            "Detected score statistics for {} that will NOT be computed for shards "
                            "at this point. Run `irk-scorestats` to do so.",
                            score_name);
                    }
                }
                for (const auto& path : shard_dirs_) {
                    boost::filesystem::create_directory(path);
                }
                titles();
                auto [avg_sizes, max_sizes] = sizes();
                auto total_occurrences = postings_once();
                Vector<ShardId, size_t> document_counts(shard_count_, 0);
                for (const auto& shard : shard_mapping_) {
                    document_counts[shard] += 1;
                }
                write_properties(
                    document_counts, avg_sizes, max_sizes, total_occurrences);
                write_reverse_mappings();
                copy_term_tables();
                return nonstd::expected<void, std::string>();
            } catch (boost::filesystem::filesystem_error& error) {
                return nonstd::make_unexpected(std::string(error.what()));
            }
        }

        inline void write_reverse_mappings() const
        {
            for (auto&& [idx, dir] : enumerate(shard_dirs_)) {
                std::ofstream os((dir / "reverse.map").string());
                io::write_vector(reverse_mapping_[ShardId(idx)].as_vector(),
                                 os);
            }
        }

        inline void copy_term_tables()
        {
            auto copy = [](auto from, auto to) {
                boost::filesystem::copy_file(
                    from, to, boost::filesystem::copy_option::overwrite_if_exists);
            };
            auto cluster_dir = shard_dirs_.begin()->parent_path();
            copy(index::term_doc_freq_path(input_dir_), index::term_doc_freq_path(cluster_dir));
            copy(index::term_occurrences_path(input_dir_),
                 index::term_occurrences_path(cluster_dir));
            copy(index::term_map_path(input_dir_), index::term_map_path(cluster_dir));
            for (auto const& [name, path] : index::find_score_stats_paths(input_dir_)) {
                if (path.max) {
                    copy(path.max.value(), cluster_dir / fmt::format("{}.max", name));
                }
                if (path.mean) {
                    copy(path.mean.value(), cluster_dir / fmt::format("{}.mean", name));
                }
                if (path.var) {
                    copy(path.var.value(), cluster_dir / fmt::format("{}.var", name));
                }
            }
        }

        inline void write_properties(const Vector<ShardId, size_t>& document_counts,
                                     const Vector<ShardId, size_t>& avg_document_sizes,
                                     const Vector<ShardId, size_t>& max_document_sizes,
                                     const Vector<ShardId, frequency_t>& total_occurrences)
        {
            auto input_props = index::Properties::read(input_dir_);

            for (auto&& [idx, dir] : enumerate(shard_dirs_)) {
                ShardId shard(idx);
                index::Properties shard_props{};
                shard_props.document_count = document_counts[shard];
                shard_props.occurrences_count = total_occurrences[shard];
                shard_props.skip_block_size = input_props.skip_block_size;
                shard_props.avg_document_size = avg_document_sizes[shard];
                shard_props.max_document_size = max_document_sizes[shard];
                index::Properties::write(shard_props, dir);
            }

            input_props.shard_count = shard_count_;
            index::Properties::write(
                input_props,
                shard_dirs_.begin()->parent_path() / "properties.json");
        }

        const int32_t shard_count_;
        const int32_t document_count_;
        const path& input_dir_;
        const Vector<ShardId, path>& shard_dirs_;
        const Vector<document_t, ShardId>& shard_mapping_;
        const Vector<document_t>& document_mapping_;
        const Vector<ShardId, Vector<document_t>>& reverse_mapping_;
    };

}  // namespace detail::partition

/// Partitions an inverted index in `input_path` into `shard_count` shards
/// described by `shard_mapping`, writing them to `output_dir`.
///
/// \param input_dir        an index directory
/// \param output_dir       a directory to write the resulting shards to
/// \param shard_mapping    `i`-th value is the shard assigned to document `i`
/// \param shard_count      total number of shards
nonstd::expected<void, std::string>
partition_index(const path& input_dir,
                const path& output_dir,
                const Vector<document_t, ShardId>& shard_mapping,
                int shard_count)
{
    int32_t document_count = shard_mapping.size();
    auto shard_dirs = detail::partition::resolve_paths(output_dir, shard_count);
    auto document_mapping = detail::partition::compute_document_mapping(shard_mapping, shard_count);
    auto reverse_mapping = detail::partition::compute_reverse_mapping(shard_mapping, shard_count);
    auto log = spdlog::get("partition");
    if (log) {
        log->info("Partitioning index {} into {} shards in {}",
                  input_dir.string(),
                  shard_count,
                  output_dir.string());
    }
    auto partition = detail::partition::Partition(shard_count,
                                                  document_count,
                                                  input_dir,
                                                  shard_dirs,
                                                  shard_mapping,
                                                  document_mapping,
                                                  reverse_mapping);
    return partition.index();
}

}  // namespace irk
