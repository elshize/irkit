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

#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gsl/span>
#include <gtest/gtest.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <irkit/index.hpp>
#include <irkit/index/assembler.hpp>
#include <irkit/index/block_inverted_list.hpp>
#include <irkit/index/partition.hpp>
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
    irk::vmap<irk::index::document_t, ShardId> shard_map;
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
        irk::score_index<
            irk::score::bm25_scorer,
            irk::inverted_index_disk_data_source>(input_dir, 8);
        output_dir = boost::filesystem::temp_directory_path() / "irkit_partition_test_shards";
        if (exists(output_dir)) remove_all(output_dir);
        boost::filesystem::create_directory(output_dir);
        auto s = [](int s) { return ShardId(s); };
        shard_map = {
            s(0), s(1), s(2), s(2), s(1), s(0), s(1), s(1), s(2), s(0)};
    }
};

using size_type = irk::inverted_index_view::size_type;

TEST_F(partition_test, resolve_paths)
{
    auto shard_paths = irk::partition::resolve_paths(output_dir, 3);
    ASSERT_EQ(shard_paths.size(), 3);
    ASSERT_THAT(
        shard_paths[0].string(),
        ::testing::StrEq(fmt::format("{}/000", output_dir.string())));
    ASSERT_THAT(
        shard_paths[1].string(),
        ::testing::StrEq(fmt::format("{}/001", output_dir.string())));
    ASSERT_THAT(
        shard_paths[2].string(),
        ::testing::StrEq(fmt::format("{}/002", output_dir.string())));
}

TEST_F(partition_test, sizes)
{
    auto shard_paths = irk::partition::resolve_paths(output_dir, 3);
    for (const auto& path : shard_paths) {
        boost::filesystem::create_directory(path);
    }
    irk::partition::sizes(input_dir, shard_paths, shard_map);
    {
        auto sizes = irk::load_compact_table<size_type>(
                         irk::index::doc_sizes_path(shard_paths[0]))
                         .to_vector();
        ASSERT_THAT(sizes, ::testing::ElementsAre(8, 10, 7));
    }
    {
        auto sizes = irk::load_compact_table<size_type>(
                         irk::index::doc_sizes_path(shard_paths[1]))
                         .to_vector();
        ASSERT_THAT(sizes, ::testing::ElementsAre(10, 7, 10, 7));
    }
    {
        auto sizes = irk::load_compact_table<size_type>(
                         irk::index::doc_sizes_path(shard_paths[2]))
                         .to_vector();
        ASSERT_THAT(sizes, ::testing::ElementsAre(9, 9, 11));
    }
}

TEST_F(partition_test, titles)
{
    auto shard_paths = irk::partition::resolve_paths(output_dir, 3);
    for (const auto& path : shard_paths) {
        boost::filesystem::create_directory(path);
    }
    irk::partition::titles(input_dir, shard_paths, shard_map);
    {
        auto titles_lex = irk::load_lexicon(
            irk::make_memory_view(irk::index::title_map_path(shard_paths[0])));
        std::vector<std::string> titles;
        irk::io::load_lines(
            irk::index::titles_path(shard_paths[0]).string(), titles);
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
        auto titles_lex = irk::load_lexicon(
            irk::make_memory_view(irk::index::title_map_path(shard_paths[1])));
        std::vector<std::string> titles;
        irk::io::load_lines(
            irk::index::titles_path(shard_paths[1]).string(), titles);
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
        auto titles_lex = irk::load_lexicon(
            irk::make_memory_view(irk::index::title_map_path(shard_paths[2])));
        std::vector<std::string> titles;
        irk::io::load_lines(
            irk::index::titles_path(shard_paths[2]).string(), titles);
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

void test_postings(
    const path& input_dir, const irk::vmap<ShardId, path>& shard_dirs)
{
    auto source =
        irk::inverted_index_mapped_data_source::from(input_dir).value();
    irk::inverted_index_view index(&source);
    std::vector<irk::inverted_index_mapped_data_source> shard_sources;
    std::vector<irk::inverted_index_view> shards;
    std::transform(
        shard_dirs.begin(),
        shard_dirs.end(),
        std::back_inserter(shard_sources),
        [](const path& dir) {
            return irk::inverted_index_mapped_data_source::from(dir).value();
        });
    std::transform(
        shard_sources.begin(),
        shard_sources.end(),
        std::back_inserter(shards),
        [](const auto& source) { return irk::inverted_index_view(&source); });
    //for (const auto& [global_id, term] : iter::enumerate(index.terms())) {
    //    auto all_documents = index.documents(term);
    //    for (const auto& shard : shards) {

    //    }
    //}
}

TEST_F(partition_test, index)
{
    auto shard_paths = irk::partition::resolve_paths(output_dir, 3);
    irk::partition::index(input_dir, output_dir, shard_map, 100, 3);
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

