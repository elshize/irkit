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

#include <gmock/gmock.h>
#include <gsl/span>
#include <gtest/gtest.h>
#include <tbb/task_scheduler_init.h>

#include <irkit/index.hpp>
#include <irkit/index/assembler.hpp>
#include <irkit/index/block_inverted_list.hpp>
#include <irkit/index/reorder.hpp>
#include <irkit/index/score.hpp>

namespace {

using boost::filesystem::exists;
using boost::filesystem::path;
using boost::filesystem::remove_all;

class reorder_test : public ::testing::Test {
protected:
    path dir;
    reorder_test()
    {
        dir = boost::filesystem::temp_directory_path() / "irkit_reorder_test";
        if (exists(dir)) remove_all(dir);
        irk::index::index_assembler assembler(dir, 100, 4, 16);
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
            "dignissim "
            "felis gravida.\n"
            "Doc09 Cras pulvinar ante in massa euismod tempor.\n");
        assembler.assemble(input);
        irk::index::score_index<
            irk::score::bm25_scorer,
            irk::inverted_index_mapped_data_source>(dir, 8);
    }
};

TEST_F(reorder_test, sizes)
{
    // given
    auto size_table = irk::build_compact_table<int32_t>(
        std::vector<int32_t>{10, 20, 30, 40, 50, 60});
    std::vector<irk::index::document_t> map{2, 0, 3, 1, 5, 4};

    // when
    auto reordered = irk::reorder::sizes(size_table, map);

    // then
    ASSERT_THAT(
        reordered,
        ::testing::ElementsAreArray(
            std::vector<int32_t>{30, 10, 40, 20, 60, 50}));
}

TEST_F(reorder_test, sizes_shorter)
{
    // given
    auto size_table = irk::build_compact_table<int32_t>(
        std::vector<int32_t>{10, 20, 30, 40, 50, 60});
    std::vector<irk::index::document_t> map{2, 0, 3, 1};

    // when
    auto reordered = irk::reorder::sizes(size_table, map);

    // then
    ASSERT_THAT(
        reordered,
        ::testing::ElementsAreArray(std::vector<int32_t>{30, 10, 40, 20}));
}

TEST_F(reorder_test, titles)
{
    // given
    auto lex = irk::build_lexicon(
        std::vector<std::string>{"a", "b", "c", "d", "e", "f"}, 16);
    std::vector<irk::index::document_t> map{2, 0, 3, 1, 5, 4};

    // when
    auto reordered = irk::reorder::titles(lex, map);

    // then
    ASSERT_EQ(reordered.size(), 6);
    ASSERT_THAT(reordered.key_at(0), ::testing::StrEq("c"));
    ASSERT_THAT(reordered.key_at(1), ::testing::StrEq("a"));
    ASSERT_THAT(reordered.key_at(2), ::testing::StrEq("d"));
    ASSERT_THAT(reordered.key_at(3), ::testing::StrEq("b"));
    ASSERT_THAT(reordered.key_at(4), ::testing::StrEq("f"));
    ASSERT_THAT(reordered.key_at(5), ::testing::StrEq("e"));
}

TEST_F(reorder_test, docmap)
{
    auto max = std::numeric_limits<irk::index::document_t>::max();
    std::vector<irk::index::document_t> permutation{2, 0, 1, 5};
    auto map = irk::reorder::docmap(permutation, 6);
    ASSERT_THAT(map, ::testing::ElementsAre(1, 2, 0, max, max, 3));
}

TEST_F(reorder_test, compute_mask)
{
    ASSERT_THAT(
        irk::reorder::compute_mask(
            std::vector<int>{0, 1, 5},
            irk::reorder::docmap(std::vector<int>{2, 0, 3, 1, 5, 4}, 6)),
        testing::ElementsAre(0, 1, 2));
    ASSERT_THAT(
        irk::reorder::compute_mask(
            std::vector<int>{0, 1, 2, 3, 4, 5},
            irk::reorder::docmap(std::vector<int>{2, 0, 3, 1, 5, 4}, 6)),
        testing::ElementsAre(2, 0, 3, 1, 5, 4));
    ASSERT_THAT(
        irk::reorder::compute_mask(
            std::vector<int>{0, 1, 2, 4, 5},
            irk::reorder::docmap(std::vector<int>{2, 0, 3, 1, 5, 4}, 6)),
        testing::ElementsAre(2, 0, 1, 4, 3));
    ASSERT_THAT(
        irk::reorder::compute_mask(
            std::vector<int>{0, 1, 2, 4, 5},
            irk::reorder::docmap(std::vector<int>{2, 0, 3}, 6)),
        testing::ElementsAre(2, 0));
    ASSERT_THAT(
        irk::reorder::compute_mask(
            std::vector<int>{0, 1, 2, 3, 4, 5},
            irk::reorder::docmap(std::vector<int>{2, 0, 3}, 6)),
        testing::ElementsAre(2, 0, 3));
}

class mock_block_list_builder
    : public irk::index::block_list_builder<int, irk::vbyte_codec<int>, false> {
public:
    MOCK_METHOD1(add, void(int));
    mock_block_list_builder()
        : block_list_builder<int, irk::vbyte_codec<int>, false>(10)
    {}
    mock_block_list_builder(const mock_block_list_builder&) = default;
};

TEST_F(reorder_test, write_score_list)
{
    mock_block_list_builder builder;
    EXPECT_CALL(builder, add(0));
    EXPECT_CALL(builder, add(1));
    EXPECT_CALL(builder, add(5));
    irk::reorder::write_score_list(
        builder,
        std::vector<int>{0, 1, 5},
        std::vector<int>{0, 1, 2},
        std::cout);
}

TEST_F(reorder_test, write_score_list_reordered)
{
    mock_block_list_builder builder;
    EXPECT_CALL(builder, add(2));
    EXPECT_CALL(builder, add(0));
    EXPECT_CALL(builder, add(1));
    EXPECT_CALL(builder, add(5));
    EXPECT_CALL(builder, add(4));
    irk::reorder::write_score_list(
        builder,
        std::vector<int>{0, 1, 2, 4, 5},
        std::vector<int>{2, 0, 1, 4, 3},
        std::cout);
}

template<typename T, typename List>
std::vector<std::pair<std::string, T>> unify_list(
    const List& postings,
    irk::lexicon<irk::hutucker_codec<char>, irk::memory_view> lexicon,
    const std::unordered_set<std::string>& blacklist)
{
    std::vector<std::pair<std::string, T>> unified;
    for (const auto& posting : postings) {
        std::string title = lexicon.key_at(posting.document());
        if (blacklist.count(title) == 0u) {
            unified.emplace_back(title, posting.payload());
        }
    }
    std::sort(
        unified.begin(), unified.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.first < rhs.first;
        });
    return unified;
}

TEST_F(reorder_test, reorder)
{
    auto output_dir =
        boost::filesystem::temp_directory_path() / "irkit_prune_test_reordered";
    if (exists(output_dir)) remove_all(output_dir);
    boost::filesystem::create_directory(output_dir);

    auto source =
        irk::inverted_index_inmemory_data_source::from(dir, {"bm25"}).value();
    irk::inverted_index_view index(&source);
    irk::reorder::index(dir, output_dir, {8, 0, 9, 5, 2, 6, 1, 4});

    auto rsource =
        irk::inverted_index_inmemory_data_source::from(output_dir, {"bm25"})
            .value();
    irk::inverted_index_view rindex(&rsource);

    std::unordered_set<std::string> removed_documents{"Doc03", "Doc07"};

    ASSERT_THAT(
        std::vector<std::string>(
            rindex.titles().begin(), rindex.titles().end()),
        ::testing::ElementsAre(
            "Doc08",
            "Doc00",
            "Doc09",
            "Doc05",
            "Doc02",
            "Doc06",
            "Doc01",
            "Doc04"));
    ASSERT_THAT(
        std::vector<std::string>(rindex.terms().begin(), rindex.terms().end()),
        ::testing::ElementsAreArray(std::vector<std::string>(
            rindex.terms().begin(), rindex.terms().end())));
    for (const auto& term : index.terms()) {
        auto expected = unify_list<irk::index::frequency_t>(
            index.postings(term), index.titles(), removed_documents);
        auto reordered = unify_list<irk::index::frequency_t>(
            rindex.postings(term), rindex.titles(), {});
        ASSERT_THAT(reordered, ::testing::ElementsAreArray(expected));
    }
    for (const auto& term : index.terms()) {
        auto expected = unify_list<irk::inverted_index_view::score_type>(
            index.scored_postings(term), index.titles(), removed_documents);
        auto reordered = unify_list<irk::inverted_index_view::score_type>(
            rindex.scored_postings(term), rindex.titles(), {});
        ASSERT_THAT(reordered, ::testing::ElementsAreArray(expected));
    }
}

}  // namespace

int main(int argc, char** argv)
{
    tbb::task_scheduler_init init(4);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
