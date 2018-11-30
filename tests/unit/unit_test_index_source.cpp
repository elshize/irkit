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

#include <functional>

#include <boost/filesystem.hpp>
#include <catch2/catch.hpp>
#include <irkit/index/source.hpp>
#include <irkit/io.hpp>

#include "common.hpp"

std::vector<char> to_vector(irk::memory_view const& view)
{
    return std::vector<char>(view.begin(), view.end());
}

std::vector<char> load(boost::filesystem::path const& file)
{
    std::vector<char> vec;
    irk::io::load_data(file, vec);
    return vec;
}

TEST_CASE("Inverted_Index_Source", "[inverted_index][unit]")
{
    GIVEN("test index")
    {
        auto dir = irk::test::tmpdir();
        std::cerr << dir.c_str() << '\n';
        irk::test::build_test_index(dir);

        WHEN("index source created")
        {
            auto source_exp = irk::Inverted_Index_In_Memory_Source::from(dir, {"bm25-8"});
            THEN("source successfully returned") { REQUIRE(source_exp.has_value()); }
            auto source = irtl::value(source_exp);
            THEN("all members can be read")
            {
                REQUIRE(to_vector(source->documents_view()) == load(irk::index::doc_ids_path(dir)));
                REQUIRE(to_vector(source->counts_view()) == load(irk::index::doc_counts_path(dir)));
                REQUIRE(to_vector(source->document_offsets_view())
                        == load(irk::index::doc_ids_off_path(dir)));
                REQUIRE(to_vector(source->count_offsets_view())
                        == load(irk::index::doc_counts_off_path(dir)));
                REQUIRE(to_vector(source->term_collection_frequencies_view())
                        == load(irk::index::term_doc_freq_path(dir)));
                REQUIRE(to_vector(source->term_collection_occurrences_view())
                        == load(irk::index::term_occurrences_path(dir)));
                REQUIRE(to_vector(source->term_map_view()) == load(irk::index::term_map_path(dir)));
                REQUIRE(to_vector(source->title_map_view())
                        == load(irk::index::title_map_path(dir)));
                REQUIRE(to_vector(source->document_sizes_view())
                        == load(irk::index::doc_sizes_path(dir)));
                REQUIRE(to_vector(source->properties_view())
                        == load(irk::index::properties_path(dir)));
            }
            THEN("default score is bm25-8")
            {
                REQUIRE(source->default_score() == "bm25-8");
            }
            THEN("score stats are present") {
                auto score_stats = source->score_stats_views();
                REQUIRE(score_stats["bm25"].max.has_value());
                REQUIRE(to_vector(score_stats["bm25"].max.value()) == load(dir / "bm25.max"));
                REQUIRE(score_stats["bm25"].mean.has_value());
                REQUIRE(to_vector(score_stats["bm25"].mean.value()) == load(dir / "bm25.mean"));
                REQUIRE(score_stats["bm25"].var.has_value());
                REQUIRE(to_vector(score_stats["bm25"].var.value()) == load(dir / "bm25.var"));
            }
            THEN("quantized scores are present") {
                auto scores = source->scores_source("bm25-8").value();
                REQUIRE(to_vector(scores.postings) == load(dir / "bm25-8.scores"));
                REQUIRE(to_vector(scores.offsets) == load(dir / "bm25-8.offsets"));
                REQUIRE(to_vector(scores.max_scores) == load(dir / "bm25-8.maxscore"));
            }
        }
    }
}

// TODO(elshize): use TEMPLATE_TEST_CASE once upgraded to Catch 2.5
TEST_CASE("Inverted_Index_Mapped_Source", "[inverted_index][unit]")
{
    GIVEN("test index")
    {
        auto dir = irk::test::tmpdir();
        std::cerr << dir.c_str() << '\n';
        irk::test::build_test_index(dir);

        WHEN("index source created")
        {
            auto source_exp = irk::Inverted_Index_Mapped_Source::from(dir, {"bm25-8"});
            THEN("source successfully returned") { REQUIRE(source_exp.has_value()); }
            auto source = irtl::value(source_exp);
            THEN("all members can be read")
            {
                REQUIRE(to_vector(source->documents_view()) == load(irk::index::doc_ids_path(dir)));
                REQUIRE(to_vector(source->counts_view()) == load(irk::index::doc_counts_path(dir)));
                REQUIRE(to_vector(source->document_offsets_view())
                        == load(irk::index::doc_ids_off_path(dir)));
                REQUIRE(to_vector(source->count_offsets_view())
                        == load(irk::index::doc_counts_off_path(dir)));
                REQUIRE(to_vector(source->term_collection_frequencies_view())
                        == load(irk::index::term_doc_freq_path(dir)));
                REQUIRE(to_vector(source->term_collection_occurrences_view())
                        == load(irk::index::term_occurrences_path(dir)));
                REQUIRE(to_vector(source->term_map_view()) == load(irk::index::term_map_path(dir)));
                REQUIRE(to_vector(source->title_map_view())
                        == load(irk::index::title_map_path(dir)));
                REQUIRE(to_vector(source->document_sizes_view())
                        == load(irk::index::doc_sizes_path(dir)));
                REQUIRE(to_vector(source->properties_view())
                        == load(irk::index::properties_path(dir)));
            }
            THEN("default score is bm25-8")
            {
                REQUIRE(source->default_score() == "bm25-8");
            }
            THEN("score stats are present") {
                auto score_stats = source->score_stats_views();
                REQUIRE(score_stats["bm25"].max.has_value());
                REQUIRE(to_vector(score_stats["bm25"].max.value()) == load(dir / "bm25.max"));
                REQUIRE(score_stats["bm25"].mean.has_value());
                REQUIRE(to_vector(score_stats["bm25"].mean.value()) == load(dir / "bm25.mean"));
                REQUIRE(score_stats["bm25"].var.has_value());
                REQUIRE(to_vector(score_stats["bm25"].var.value()) == load(dir / "bm25.var"));
            }
            THEN("quantized scores are present") {
                auto scores = source->scores_source("bm25-8").value();
                REQUIRE(to_vector(scores.postings) == load(dir / "bm25-8.scores"));
                REQUIRE(to_vector(scores.offsets) == load(dir / "bm25-8.offsets"));
                REQUIRE(to_vector(scores.max_scores) == load(dir / "bm25-8.maxscore"));
            }
        }
    }
}
