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

#include <sstream>

#include <boost/filesystem.hpp>
#include <catch2/catch.hpp>

#include <irkit/index.hpp>
#include <irkit/index/score.hpp>
#include <irkit/index/source.hpp>
#include <irkit/io.hpp>
#include "common.hpp"

using namespace irk::test;
using irk::index::detail::StatTuple;
using Catch::Matchers::Equals;
using float_codec = irk::copy_codec<double>;

TEST_CASE("Unzip a StatTuple vector", "[scorestats]")
{
    std::vector<StatTuple> tuples = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    SECTION("max")
    {
        auto max_vec = irk::index::detail::unzip(
            tuples, [](const auto& t) { return t.max; });
        REQUIRE_THAT(max_vec, Equals(std::vector<float>{1, 4, 7}));
    }
}

TEST_CASE("Write float table", "[scorestats]")
{
    auto test_dir = tmpdir();
    auto table_path = test_dir / "float.table";
    GIVEN("A vector of floats") {
        std::vector<float> floats = {1, 4, 7};
        WHEN("Write table to file")
        {
            irk::index::detail::write_table(floats, table_path);
            THEN("Loaded table has the same elements") {
                auto loaded = irk::io::read_vector<float>(table_path);
                REQUIRE_THAT(loaded, Equals(floats));
            }
        }
    }
}

TEST_CASE("Score stats for index", "[scorestats]")
{
    GIVEN("A mock inverted index")
    {
        struct MockPosting {
            irk::index::document_t doc_;
            double score_;
            auto document() const { return doc_; }
            auto score() const { return score_; }
        };
        struct MockIndex {
            std::vector<std::vector<MockPosting>> postings{
                {{0, 12.0}},
                {{0, 12.0}, {1, 24.0}},
                {{0, 12.0}, {1, 24.0}, {2, 36.0}}};
        } index;

        WHEN("Calculated BM25 statistics")
        {
            auto dir = tmpdir();
            irk::index::detail::ScoreStatsFn fn{dir, "bm25"};
            fn(std::vector<int>{0, 1, 2},
               [&](auto term_id) { return index.postings[term_id]; });
            THEN("Stored statistics are correct")
            {
                auto max = irk::io::read_vector<float>(dir / "bm25.max");
                auto mean = irk::io::read_vector<float>(dir / "bm25.mean");
                auto var = irk::io::read_vector<float>(dir / "bm25.var");
                REQUIRE_THAT(max, Equals(std::vector<float>{12, 24, 36}));
                REQUIRE_THAT(mean, Equals(std::vector<float>{12, 18, 24}));
                REQUIRE_THAT(var, Equals(std::vector<float>{0, 36, 96}));
            }
        }
    }
}
