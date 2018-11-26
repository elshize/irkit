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

#include <catch2/catch.hpp>

#include <irkit/index/types.hpp>
#include <irkit/iterator/block_iterator.hpp>
#include <irkit/list/standard_block_list.hpp>
#include <irkit/list/vector_block_list.hpp>

using ir::Block_Iterator;
using ir::Blocked_Position;
using ir::Standard_Block_Document_List;
using ir::Standard_Block_List;
using ir::Standard_Block_List_Builder;
using ir::Standard_Block_Payload_List;
using ir::Vector_Block_List;
using irk::index::document_t;

TEST_CASE("Standard_Block_List", "[blocked][inverted_list]")
{
    auto vec            = std::vector<int>{1, 5, 6, 8, 12, 14, 20, 23};
    using list_type     = Standard_Block_List<int, irk::vbyte_codec<int>, true>;
    ir::Standard_Block_List_Builder<int, irk::vbyte_codec<int>, true> builder{3};
    for (auto v : vec) {
        builder.add(v);
    }
    std::ostringstream os;
    builder.write(os);
    std::string data = os.str();
    list_type list{0, irk::make_memory_view(data.data(), data.size()), 8};

    SECTION("size") { REQUIRE(list.size() == 8); }
    SECTION("block count") { REQUIRE(list.block_count() == 3); }
    SECTION("term ID") { REQUIRE(list.term_id() == 0); }
    SECTION("upper bounds") { REQUIRE(list.upper_bounds() == std::vector<int>{6, 14, 23}); }

    SECTION("block size")
    {
        REQUIRE(list.block_size() == 3);
        auto [n, size] = GENERATE(
            table<int, list_type::size_type>({{0, 3}, {1, 3}, {2, 2}}));
        REQUIRE(list.block_size(n) == size);
    }

    SECTION("construct vector from iterators")
    {
        std::vector<int> constructed(list.begin(), list.end());
        REQUIRE(constructed == vec);
    }

    SECTION("copy constructor")
    {
        auto other = list;
        std::vector<int> constructed_copy(other.begin(), other.end());
        REQUIRE(constructed_copy == vec);
        std::vector<int> constructed(list.begin(), list.end());
        REQUIRE(constructed == vec);
    }

    SECTION("move constructor")
    {
        auto other = std::move(list);
        std::vector<int> constructed_copy(other.begin(), other.end());
        REQUIRE(constructed_copy == vec);
    }

    SECTION("block")
    {
        auto [n, expected_block] = GENERATE(
            table<int, std::vector<int>>({{0, {1, 5, 6}}, {1, {8, 12, 14}}, {2, {20, 23}}}));
        auto block_span = list.block(n);
        REQUIRE(std::vector<int>(block_span.begin(), block_span.end()) == expected_block);
    }
}

TEST_CASE("Standard_Block_List_Builder", "[blocked][inverted_list][builder]")
{
    constexpr auto vb = [](auto n) { return n | (char)0b10000000; };
    int block_size = 2;

    std::vector<document_t> documents = {9, 11, 12, 22, 27};
    std::vector<std::int32_t> payloads = {9, 2, 1, 10, 5};

    SECTION("Write documents")
    {
        std::vector<char> memory = {
            5,
            127,
            -128, /* some random bytes before posting list */

            /* The following will be read and decoded on construction of a view */
            vb(14), /* size of the memory in bytes */
            vb(2), /* block size */
            vb(3), /* number of blocks */
            vb(0),
            vb(2),
            vb(2), /* skips (relative to previous block) */
            vb(11),
            vb(11),
            vb(5), /* last values in blocks (delta encoded) */

            /* The following (ID gaps) will be read lazily */
            vb(9),
            vb(2),
            vb(1),
            vb(10),
            vb(5),

            5,
            127,
            -128 /* some random bytes after posting list */
        };
        Standard_Block_List_Builder<document_t, irk::vbyte_codec<document_t>, true> builder{
            block_size};
        std::ostringstream buffer;
        for (auto doc : documents) {
            builder.add(doc);
        }
        builder.write(buffer);

        std::vector<char> expected(memory.begin() + 3, memory.end() - 3);

        auto str = buffer.str();
        REQUIRE(std::vector<char>(str.begin(), str.end()) == expected);
    }
    SECTION("Write payloads") {
        std::vector<char> memory = {
            5,
            127,
            -128, /* some random bytes before posting list */

            /* The following will be read and decoded on construction of a view */
            vb(11), /* size of the memory in bytes */
            vb(2), /* block size */
            vb(3), /* number of blocks */
            vb(0),
            vb(2),
            vb(2), /* skips (relative to previous block) */

            /* The following payload will be read lazily */
            vb(9),
            vb(2),
            vb(1),
            vb(10),
            vb(5),

            5,
            127,
            -128 /* some random bytes after posting list */
        };
        Standard_Block_List_Builder<int, irk::vbyte_codec<int>, false> builder{block_size};
        std::ostringstream buffer;
        for (int pay : payloads) {
            builder.add(pay);
        }
        builder.write(buffer);

        std::vector<char> expected(memory.begin() + 3, memory.end() - 3);

        auto str = buffer.str();
        REQUIRE(std::vector<char>(str.begin(), str.end()) == expected);
    }
}

// Re-enable after fixing #77
TEST_CASE("Standard_Block_List_Builder from file", "[.][blocked][inverted_list][builder]")
{

    Standard_Block_List_Builder<document_t, irk::vbyte_codec<document_t>, true> builder{64};
    std::cerr << boost::filesystem::path("doclist.txt").c_str() << '\n';
    std::ifstream in("doclist.txt");
    std::vector<document_t> documents;
    std::string line;
    while (std::getline(in, line)) {
        documents.push_back(std::stoi(line));
        builder.add(documents.back());
    }
    REQUIRE(documents.size() == 145280u);
    REQUIRE(documents == builder.values());

    std::ostringstream buffer;
    builder.write(buffer);
    auto s = buffer.str();
    std::vector<char> data(s.begin(), s.end());
    Standard_Block_Document_List<irk::vbyte_codec<document_t>> view(
        0, irk::make_memory_view(data), documents.size());

    document_t prev = 0;
    const int32_t num_blocks = (documents.size() + 63) / 64;
    std::vector<document_t> all_decoded;
    for (int block = 0; block < num_blocks; ++block) {
        const gsl::index begin_idx = 64 * block;
        const gsl::index end_idx = std::min(begin_idx + 64, gsl::index(documents.size()));
        auto count = end_idx - begin_idx;
        std::vector<document_t> block_documents(&documents[begin_idx], &documents[end_idx]);
        std::vector<char> expected_data = irk::delta_encode(
            irk::vbyte_codec<document_t>{}, block_documents.begin(), block_documents.end(), prev);
        std::vector<char> actual_data(view.block(block).data(),
                                      view.block(block).data() + view.block(block).size());
        REQUIRE(actual_data == expected_data);
        std::vector<document_t> decoded(count);
        irk::vbyte_codec<document_t>{}.delta_decode(
            actual_data.begin(), decoded.begin(), count, prev);
        REQUIRE(decoded == block_documents);
        prev = documents[end_idx - 1];
        all_decoded.insert(all_decoded.end(), decoded.begin(), decoded.end());
    }
    REQUIRE(all_decoded == documents);
    std::vector<document_t> decoded_iter(view.begin(), view.end());
    REQUIRE(decoded_iter == documents);
}
