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

#include <algorithm>
#include <random>
#include <sstream>
#include <unordered_set>
#include <vector>

#include <boost/iostreams/device/mapped_file.hpp>
#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gsl/span>
#include <gtest/gtest.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <irkit/index.hpp>
#include <irkit/index/assembler.hpp>
#include <irkit/index/block_inverted_list.hpp>
#include <irkit/index/partition.hpp>
#include <irkit/index/score.hpp>
#include <irkit/index/types.hpp>

namespace {

using boost::filesystem::exists;
using boost::filesystem::path;
using boost::filesystem::remove_all;

using irk::index::document_t;
using irk::index::frequency_t;
using irk::index::term_id_t;
using irk::ShardId;

class partition_test : public ::testing::Test {
protected:
    path input_dir;
    path output_dir;
    irk::vmap<ShardId, path> shard_dirs;
    irk::vmap<irk::index::document_t, ShardId> shard_map;
    std::vector<document_t> document_mapping;
    partition_test()
    {
        input_dir = boost::filesystem::temp_directory_path() / "irkit_partition_test";
        if (exists(input_dir)) remove_all(input_dir);
        irk::index::index_assembler assembler(input_dir, 100, 4, 16);
        std::istringstream input(
            "Doc00 Lorem ipsum dolor sit amet, consectetur adipiscing elit.\n"
            "Doc01 Proin ullamcorper nunc et odio suscipit, eu placerat metus "
            "vestibulum.\n"
            "Doc02 Mauris non ipsum feugiat, aliquet libero eget, gravida "
            "dolor.\n"
            "Doc03 Nullam non ipsum hendrerit, malesuada tellus sed, placerat "
            "ante.\n"
            "Doc04 Donec aliquam sapien imperdiet libero semper bibendum.\n"
            "Doc05 Nam lacinia libero at nunc tincidunt, in ullamcorper ipsum "
            "fermentum.\n"
            "Doc06 Aliquam vel ante id dolor dignissim vehicula in at leo.\n"
            "Doc07 Maecenas mollis mauris vitae enim pretium ultricies.\n"
            "Doc08 Vivamus bibendum ligula sit amet urna scelerisque, eget "
            "dignissim felis gravida.\n"
            "Doc09 Cras pulvinar ante in massa euismod tempor.\n");
        assembler.assemble(input);
        irk::index::score_index<
            irk::score::bm25_tag,
            irk::inverted_index_mapped_data_source>(input_dir, 8);
        output_dir = boost::filesystem::temp_directory_path() / "irkit_partition_test_shards";
        if (exists(output_dir)) remove_all(output_dir);
        boost::filesystem::create_directory(output_dir);
        auto s = [](int s) { return ShardId(s); };
        shard_map = {
            s(0), s(1), s(2), s(2), s(1), s(0), s(1), s(1), s(2), s(0)};
        shard_dirs = irk::detail::partition::resolve_paths(output_dir, 3);
        document_mapping =
            irk::detail::partition::compute_document_mapping(shard_map, 3);
    }

    irk::detail::partition::Partition partition()
    {
        return irk::detail::partition::Partition(
            3, 10, input_dir, shard_dirs, shard_map, document_mapping);
    }
};

using size_type = irk::inverted_index_view::size_type;

TEST_F(partition_test, resolve_paths)
{
    auto shard_paths = irk::detail::partition::resolve_paths(output_dir, 3);
    ASSERT_EQ(shard_paths.size(), 3);
    ASSERT_THAT(
        shard_paths[ShardId(0)].string(),
        ::testing::StrEq(fmt::format("{}/000", output_dir.string())));
    ASSERT_THAT(
        shard_paths[ShardId(1)].string(),
        ::testing::StrEq(fmt::format("{}/001", output_dir.string())));
    ASSERT_THAT(
        shard_paths[ShardId(2)].string(),
        ::testing::StrEq(fmt::format("{}/002", output_dir.string())));
}

TEST_F(partition_test, sizes)
{
    auto part = partition();
    part.sizes();
    {
        auto sizes =
            irk::load_compact_table<size_type>(
                irk::index::doc_sizes_path(part.shard_dirs_[ShardId(0)]))
                .to_vector();
        ASSERT_THAT(sizes, ::testing::ElementsAre(8, 10, 7));
    }
    {
        auto sizes =
            irk::load_compact_table<size_type>(
                irk::index::doc_sizes_path(part.shard_dirs_[ShardId(1)]))
                .to_vector();
        ASSERT_THAT(sizes, ::testing::ElementsAre(10, 7, 10, 7));
    }
    {
        auto sizes =
            irk::load_compact_table<size_type>(
                irk::index::doc_sizes_path(part.shard_dirs_[ShardId(2)]))
                .to_vector();
        ASSERT_THAT(sizes, ::testing::ElementsAre(9, 9, 11));
    }
}

TEST_F(partition_test, titles)
{
    auto part = partition();
    part.titles();
    {
        boost::iostreams::mapped_file_source m(
            irk::index::title_map_path(part.shard_dirs_[ShardId(0)]).string());
        auto titles_lex =
            irk::load_lexicon(irk::make_memory_view(m.data(), m.size()));
        std::vector<std::string> titles;
        irk::io::load_lines(
            irk::index::titles_path(part.shard_dirs_[ShardId(0)]).string(),
            titles);
        ASSERT_EQ(titles_lex.size(), titles.size());
        ASSERT_EQ(titles_lex.size(), 3);
        ASSERT_THAT(titles[0], ::testing::StrEq("Doc00"));
        ASSERT_THAT(titles_lex.key_at(0), ::testing::StrEq("Doc00"));
        ASSERT_THAT(titles[1], ::testing::StrEq("Doc05"));
        ASSERT_THAT(titles_lex.key_at(1), ::testing::StrEq("Doc05"));
        ASSERT_THAT(titles[2], ::testing::StrEq("Doc09"));
        ASSERT_THAT(titles_lex.key_at(2), ::testing::StrEq("Doc09"));
    }
    {
        boost::iostreams::mapped_file_source m(
            irk::index::title_map_path(part.shard_dirs_[ShardId(1)]).string());
        auto titles_lex =
            irk::load_lexicon(irk::make_memory_view(m.data(), m.size()));
        std::vector<std::string> titles;
        irk::io::load_lines(
            irk::index::titles_path(part.shard_dirs_[ShardId(1)]).string(),
            titles);
        ASSERT_EQ(titles_lex.size(), titles.size());
        ASSERT_EQ(titles_lex.size(), 4);
        ASSERT_THAT(titles[0], ::testing::StrEq("Doc01"));
        ASSERT_THAT(titles_lex.key_at(0), ::testing::StrEq("Doc01"));
        ASSERT_THAT(titles[1], ::testing::StrEq("Doc04"));
        ASSERT_THAT(titles_lex.key_at(1), ::testing::StrEq("Doc04"));
        ASSERT_THAT(titles[2], ::testing::StrEq("Doc06"));
        ASSERT_THAT(titles_lex.key_at(2), ::testing::StrEq("Doc06"));
        ASSERT_THAT(titles[3], ::testing::StrEq("Doc07"));
        ASSERT_THAT(titles_lex.key_at(3), ::testing::StrEq("Doc07"));
    }
    {
        boost::iostreams::mapped_file_source m(
            irk::index::title_map_path(part.shard_dirs_[ShardId(2)]).string());
        auto titles_lex =
            irk::load_lexicon(irk::make_memory_view(m.data(), m.size()));
        std::vector<std::string> titles;
        irk::io::load_lines(
            irk::index::titles_path(part.shard_dirs_[ShardId(2)]).string(),
            titles);
        ASSERT_EQ(titles_lex.size(), titles.size());
        ASSERT_EQ(titles_lex.size(), 3);
        ASSERT_THAT(titles[0], ::testing::StrEq("Doc02"));
        ASSERT_THAT(titles_lex.key_at(0), ::testing::StrEq("Doc02"));
        ASSERT_THAT(titles[1], ::testing::StrEq("Doc03"));
        ASSERT_THAT(titles_lex.key_at(1), ::testing::StrEq("Doc03"));
        ASSERT_THAT(titles[2], ::testing::StrEq("Doc08"));
        ASSERT_THAT(titles_lex.key_at(2), ::testing::StrEq("Doc08"));
    }
}

TEST_F(partition_test, compute_document_mapping)
{
    std::vector<document_t> docmap =
        irk::detail::partition::compute_document_mapping(shard_map, 3);
    ASSERT_THAT(docmap, ::testing::ElementsAre(0, 0, 0, 1, 1, 1, 2, 3, 2, 2));
}

void test_props(
    const path& input_dir, const irk::vmap<ShardId, path>& shard_dirs)
{
    auto source =
        irk::inverted_index_mapped_data_source::from(input_dir).value();
    irk::inverted_index_view index(&source);
    size_t cumulative(0);
    size_t cumulative_occ(0);
    for (const path& dir : shard_dirs) {
        auto shard_source =
            irk::inverted_index_mapped_data_source::from(dir).value();
        irk::inverted_index_view shard(&shard_source);
        std::ifstream prop_stream(irk::index::properties_path(dir).string());
        nlohmann::json properties;
        prop_stream >> properties;
        cumulative += static_cast<size_t>(properties["documents"]);
        cumulative_occ += static_cast<size_t>(properties["occurrences"]);
    }
    ASSERT_EQ(cumulative, index.collection_size());
    ASSERT_EQ(cumulative_occ, index.occurrences_count());
}

void test_term_frequencies(
    const path& input_dir,
    const irk::vmap<ShardId, path>& shard_dirs)
{
    auto source =
        irk::inverted_index_mapped_data_source::from(input_dir).value();
    irk::inverted_index_view index(&source);
    std::vector<frequency_t> cumulative(index.term_count(), 0);
    std::vector<frequency_t> cumulative_occ(index.term_count(), 0);
    for (const path& dir : shard_dirs) {
        auto shard_source =
            irk::inverted_index_mapped_data_source::from(dir).value();
        irk::inverted_index_view shard(&shard_source);
        for (const auto& [global_id, term] : iter::enumerate(index.terms())) {
            if (auto local_id = shard.term_id(term); local_id) {
                cumulative[global_id] += shard.tdf(local_id.value());
                cumulative_occ[global_id] +=
                    shard.term_occurrences(local_id.value());
            }
        }
    }
    auto original = index.term_collection_frequencies();
    ASSERT_THAT(
        cumulative,
        ::testing::ElementsAreArray(original.begin(), original.end()));
    auto original_occ = index.term_collection_occurrences();
    ASSERT_THAT(
        cumulative_occ,
        ::testing::ElementsAreArray(original_occ.begin(), original_occ.end()));
}

MATCHER_P2(
    IsBetween,
    a,
    b,
    std::string(negation ? "isn't" : "is") + " between "
        + ::testing::PrintToString(a) + " and " + ::testing::PrintToString(b))
{
    return a <= arg && arg <= b;
}

namespace a = boost::accumulators;
using accumulator = a::accumulator_set<
    irk::inverted_index_view::score_type,
    a::stats<a::tag::mean, a::tag::variance, a::tag::max>>;

void test_postings(
    const path& input_dir, const irk::vmap<ShardId, path>& shard_dirs)
{
    auto source =
        irk::inverted_index_mapped_data_source::from(input_dir, {"bm25"})
            .value();
    irk::inverted_index_view index(&source);
    std::vector<irk::inverted_index_mapped_data_source> shard_sources;
    std::vector<irk::inverted_index_view> shards;
    std::transform(
        shard_dirs.begin(),
        shard_dirs.end(),
        std::back_inserter(shard_sources),
        [](const path& dir) {
            return irk::inverted_index_mapped_data_source::from(dir, {"bm25"})
                .value();
        });
    int sh = 0;
    std::transform(
        shard_sources.begin(),
        shard_sources.end(),
        std::back_inserter(shards),
        [&sh](const auto& source) {
            return irk::inverted_index_view(&source);
        });
    std::vector<std::vector<uint32_t>> max_scores;
    irk::transform_range(
        shards, std::back_inserter(max_scores), [](const auto& shard) {
            return shard.score_data("bm25").max_scores.to_vector();
        });
    // TODO: do with doubles
    //auto exp_values =
    //    iter::imap(
    //        [](const auto& shard) {
    //            return shard.score_data("bm25").exp_values.to_vector();
    //        },
    //        shards)
    //    | irk::collect();
    //auto vars = iter::imap(
    //                [](const auto& shard) {
    //                    return shard.score_data("bm25").variances.to_vector();
    //                },
    //                shards)
    //    | irk::collect();
    for (const auto& term : index.terms()) {
        auto original_documents = index.documents(term) | irk::collect();
        auto original_frequencies = index.frequencies(term) | irk::collect();
        auto original_scores = index.scores(term) | irk::collect();
        std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> merged;
        int shard_id = 0;
        for (const auto& shard : shards) {
            if (not shard.term_id(term).has_value()) {
                shard_id++;
                continue;
            }
            auto term_id = shard.term_id(term).value();
            auto posting_list = shard.postings(term);
            auto scored_list = shard.scored_postings(term);
            auto shard_postings = irk::collect(iter::imap(
                [&index, &shard, term](const auto& pair) {
                    const auto& [fp, sp] = pair;
                    assert(fp.document() == sp.document());
                    std::string title = shard.titles().key_at(fp.document());
                    auto global_id = index.titles().index_at(title).value();
                    return std::make_tuple(
                        global_id, fp.payload(), sp.payload());
                },
                zip(posting_list, scored_list)));
            accumulator acc;
            auto scores = iter::imap(
                [](const auto& tup) { return std::get<2>(tup); },
                shard_postings);
            std::for_each(scores.begin(), scores.end(), [&acc](auto score) {
                acc(score);
            });
            auto max = a::max(acc);
            // TODO: do with doubles
            //auto exp = a::mean(acc);
            //auto var = a::variance(acc);
            merged.insert(
                merged.end(), shard_postings.begin(), shard_postings.end());
            EXPECT_EQ(max_scores[shard_id][term_id], max);
            // TODO: do with doubles
            // EXPECT_THAT(
            //    exp_values[shard_id][term_id], IsBetween(exp - 1, exp + 1));
            // EXPECT_THAT(vars[shard_id][term_id], IsBetween(var - 1, var +
            // 1));
            ++shard_id;
        }
        std::sort(
            merged.begin(), merged.end(), [](const auto& lhs, const auto& rhs) {
                return std::get<0>(lhs) < std::get<0>(rhs);
            });
        auto get_first = [](const auto& tup) { return std::get<0>(tup); };
        auto get_second = [](const auto& tup) { return std::get<1>(tup); };
        auto get_third = [](const auto& tup) { return std::get<2>(tup); };
        auto merged_documents = irk::collect(iter::imap(get_first, merged));
        auto merged_frequencies = irk::collect(iter::imap(get_second, merged));
        auto merged_scores = irk::collect(iter::imap(get_third, merged));
        ASSERT_THAT(
            merged_documents, ::testing::ElementsAreArray(original_documents));
        ASSERT_THAT(
            merged_frequencies,
            ::testing::ElementsAreArray(original_frequencies));
        ASSERT_THAT(
            merged_scores, ::testing::ElementsAreArray(original_scores));
    }
}

TEST_F(partition_test, index)
{
    irk::partition_index(input_dir, output_dir, shard_map, 3, 10);
    auto shard_paths = irk::detail::partition::resolve_paths(output_dir, 3);
    test_props(input_dir, shard_paths);
    test_term_frequencies(input_dir, shard_paths);
    test_postings(input_dir, shard_paths);
}

}  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
