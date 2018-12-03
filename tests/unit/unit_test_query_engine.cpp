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
#include <irkit/query_engine.hpp>

#include "common.hpp"

TEST_CASE("Query_Engine", "[query_engine][unit]")
{
    auto score_function = GENERATE(std::string("bm25"), std::string("bm25-8"), std::string("ql"));
    auto traversal = GENERATE(irk::Traversal_Type::TAAT, irk::Traversal_Type::DAAT);
    std::vector<std::string> query{"ipsum"};
    GIVEN("a test index")
    {
        auto dir = irk::test::tmpdir();
        irk::test::build_test_index(dir, true, false);
        std::vector<std::string> scores;
        if (irk::Query_Engine::is_quantized(score_function)) {
            scores.push_back(score_function);
        }
        irk::inverted_index_view index{
            irk::Inverted_Index_Mapped_Source::from(dir, scores).value()};
        SECTION("run a query with a query engine")
        {
            std::vector<int> docs;
            auto engine = irk::Query_Engine::from(
                index, false, score_function, traversal, std::optional<int>{}, "null");
            engine.run_query(query, 2).print([&](auto rank, auto doc, auto score) {
                docs.push_back(doc);
            });
            THEN("got correct results") { REQUIRE(docs == std::vector<int>{0, 2}); }
        }
    }
}
