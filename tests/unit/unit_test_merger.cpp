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

#include <irkit/index/assembler.hpp>
#include <irkit/index/merger.hpp>

TEST_CASE("Merge sizes", "[merger]")
{
    GIVEN("Two mock indices")
    {
        struct MockIndex {
        public:
            int32_t collection_size_;
            std::vector<int32_t> document_sizes_;
            int32_t collection_size() const { return collection_size_; }
            const std::vector<int32_t>& document_sizes() const
            {
                return document_sizes_;
            }
        };

        std::vector<MockIndex> indices;
        indices.push_back({3, {12, 500, 2147483646}});
        indices.push_back({3, {4, 1, 2}});

        WHEN("Sizes merged")
        {
            std::ostringstream out;
            auto [document_count, avg_doc_size, max_doc_size] =
                irk::index::detail::merge_sizes(indices, out);
            THEN("Returned aggregates are correct")
            {
                REQUIRE(document_count == 6);
                REQUIRE(avg_doc_size == 357914027.5);
                REQUIRE(max_doc_size == 2147483646);
            }
        }
    }
}
