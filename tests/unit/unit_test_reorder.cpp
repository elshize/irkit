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

#define CATCH_CONFIG_MAIN

#include <algorithm>
#include <random>
#include <sstream>
#include <unordered_set>
#include <vector>

#include <catch2/catch.hpp>
#include <fakeit.hpp>
#include <gsl/span>
#include <tbb/task_scheduler_init.h>

#include <irkit/index.hpp>
#include <irkit/index/assembler.hpp>
#include <irkit/index/reorder.hpp>
#include <irkit/index/score.hpp>
#include <irkit/list/standard_block_list.hpp>
#include "common.hpp"

using boost::filesystem::exists;
using boost::filesystem::path;
using boost::filesystem::remove_all;

using namespace fakeit;

template<typename T, typename List>
std::vector<std::pair<std::string, T>>
unify_list(const List& postings,
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
    std::sort(unified.begin(), unified.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first < rhs.first;
    });
    return unified;
}

TEST_CASE("Reorder", "[reorder][unit]")
{
    SECTION("Size")
    {
        auto size_table = irk::build_compact_table<int32_t>(
            std::vector<int32_t>{10, 20, 30, 40, 50, 60});
        GIVEN("exhaustive mapping")
        {
            std::vector<irk::index::document_t> map{2, 0, 3, 1, 5, 4};
            WHEN("reorder sizes")
            {
                auto reordered = irk::reorder::sizes(size_table, map);
                THEN("equal expected of full size")
                {
                    REQUIRE(std::vector<int32_t>(reordered.begin(), reordered.end())
                            == std::vector<int32_t>{30, 10, 40, 20, 60, 50});
                }
            }
        }
        GIVEN("non-exhaustive mapping")
        {
            std::vector<irk::index::document_t> map{2, 0, 3, 1};
            WHEN("reorder sizes")
            {
                auto reordered = irk::reorder::sizes(size_table, map);
                THEN("equal expected with shorter length")
                {
                    REQUIRE(std::vector<int32_t>(reordered.begin(), reordered.end())
                            == std::vector<int32_t>{30, 10, 40, 20});
                }
            }
        }
    }

    GIVEN("a lexicon and a mapping")
    {
        auto lex = irk::build_lexicon(std::vector<std::string>{"a", "b", "c", "d", "e", "f"}, 16);
        std::vector<irk::index::document_t> map{2, 0, 3, 1, 5, 4};
        WHEN("reorder titles")
        {
            auto reordered = irk::reorder::titles(lex, map);
            THEN("titles are reoredered correctly")
            {
                REQUIRE(reordered.size() == 6);
                REQUIRE(reordered.key_at(0) == "c");
                REQUIRE(reordered.key_at(1) == "a");
                REQUIRE(reordered.key_at(2) == "d");
                REQUIRE(reordered.key_at(3) == "b");
                REQUIRE(reordered.key_at(4) == "f");
                REQUIRE(reordered.key_at(5) == "e");
            }
        }
    }

    SECTION("Document map")
    {
        auto max = std::numeric_limits<irk::index::document_t>::max();
        std::vector<irk::index::document_t> permutation{2, 0, 1, 5};
        auto map = irk::reorder::docmap(permutation, 6);
        REQUIRE(map == std::vector<irk::index::document_t>{1, 2, 0, max, max, 3});
    }

    SECTION("Compute mask")
    {
        auto [documents, permutation, expected] = GENERATE(
            table<std::vector<int>, std::vector<int>, std::vector<int>>(
                {{{0, 1, 5}, {2, 0, 3, 1, 5, 4}, {0, 1, 2}},
                 {{0, 1, 2, 3, 4, 5}, {2, 0, 3, 1, 5, 4}, {2, 0, 3, 1, 5, 4}},
                 {{0, 1, 2, 4, 5}, {2, 0, 3, 1, 5, 4}, {2, 0, 1, 4, 3}},
                 {{0, 1, 2, 4, 5}, {2, 0, 3}, {2, 0}},
                 {{0, 1, 2, 3, 4, 5}, {2, 0, 3}, {2, 0, 3}}}));
        REQUIRE(irk::reorder::compute_mask(documents, irk::reorder::docmap(permutation, 6))
                == expected);
    }
    SECTION("Write score list")
    {
        struct BuilderInterface {
            virtual void add(int) = 0;
            virtual auto write(std::ostream&) -> std::streamsize = 0;
        };
        Mock<BuilderInterface> builder;
        Fake(Method(builder, add), Method(builder, write));
        GIVEN("a trivial order")
        {
            irk::reorder::write_score_list(
                builder.get(), std::vector<int>{0, 1, 5}, std::vector<int>{0, 1, 2}, std::cout);
            Verify(Method(builder, add).Using(0),
                   Method(builder, add).Using(1),
                   Method(builder, add).Using(5));
            VerifyNoOtherInvocations(Method(builder, add));
        }
        GIVEN("mapping that reorders documents")
        {
            irk::reorder::write_score_list(builder.get(),
                                           std::vector<int>{0, 1, 2, 4, 5},
                                           std::vector<int>{2, 0, 1, 4, 3},
                                           std::cout);
            Verify(Method(builder, add).Using(2),
                   Method(builder, add).Using(0),
                   Method(builder, add).Using(1),
                   Method(builder, add).Using(5),
                   Method(builder, add).Using(4));
            VerifyNoOtherInvocations(Method(builder, add));
        }
    }
}

TEST_CASE("Reorder index", "[reorder][unit]")
{
    GIVEN("A test index")
    {
        auto dir = irk::test::tmpdir();
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
        irk::index::score_index<irk::score::bm25_tag, irk::inverted_index_mapped_data_source>(dir, 8);

        WHEN("index is reordered and loaded")
        {
            auto output_dir = irk::test::tmpdir();
            auto source = irk::inverted_index_inmemory_data_source::from(dir, {"bm25-8"}).value();
            irk::inverted_index_view index(&source);
            irk::reorder::index(dir, output_dir, {8, 0, 9, 5, 2, 6, 1, 4});

            auto rsource = irk::inverted_index_inmemory_data_source::from(output_dir, {"bm25-8"})
                               .value();
            irk::inverted_index_view rindex(&rsource);
            std::unordered_set<std::string> removed_documents{"Doc03", "Doc07"};

            THEN("terms are the same as original")
            {
                REQUIRE(std::vector<std::string>(rindex.terms().begin(), rindex.terms().end())
                        == std::vector<std::string>(index.terms().begin(), index.terms().end()));
            }
            THEN("titles are correctly reordered")
            {
                REQUIRE(
                    std::vector<std::string>(rindex.titles().begin(), rindex.titles().end())
                    == std::vector<std::string>{
                           "Doc08", "Doc00", "Doc09", "Doc05", "Doc02", "Doc06", "Doc01", "Doc04"});
            }
            THEN("frequency postings are correct")
            {
                for (const auto& term : index.terms()) {
                    auto expected = unify_list<irk::index::frequency_t>(
                        index.postings(term), index.titles(), removed_documents);
                    auto reordered = unify_list<irk::index::frequency_t>(
                        rindex.postings(term), rindex.titles(), {});
                    REQUIRE(reordered == expected);
                }
            }
            THEN("scored postings are correct")
            {
                for (const auto& term : index.terms()) {
                    auto expected = unify_list<irk::inverted_index_view::score_type>(
                        index.scored_postings(term), index.titles(), removed_documents);
                    auto reordered = unify_list<irk::inverted_index_view::score_type>(
                        rindex.scored_postings(term), rindex.titles(), {});
                    REQUIRE(reordered == expected);
                }
            }
        }
    }
}
