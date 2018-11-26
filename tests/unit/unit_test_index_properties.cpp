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

/// \file
/// \author     Michal Siedlaczek
/// \copyright  MIT License

#include <string>

#include <boost/filesystem.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <irkit/index.hpp>

namespace {

using boost::filesystem::create_directory;
using boost::filesystem::exists;
using boost::filesystem::path;
using boost::filesystem::remove_all;

class PropertiesTest : public ::testing::Test {
protected:
    path index_dir;
    irk::index::Properties properties;
    std::string serialized = R"({
        "skip_block_size": 64,
        "occurrences": 654321,
        "documents": 10000,
        "avg_document_size": 55.5,
        "max_document_size": 20123,
        "quantized_scores": {
            "bm25-8": {
                "type": "bm25",
                "bits": 8,
                "min": 0.0,
                "max": 25.1
            },
            "bm25-24": {
                "type": "bm25",
                "bits": 24,
                "min": 0.0,
                "max": 25.1
            }
        }
    })";
    PropertiesTest() {
        index_dir =
            boost::filesystem::temp_directory_path() / "irkit_Properties_test";
        if (exists(index_dir)) {
            remove_all(index_dir);
        }
        create_directory(index_dir);
        properties.skip_block_size = 64;
        properties.occurrences_count = 654321;
        properties.document_count = 10000;
        properties.avg_document_size = 55.5;
        properties.max_document_size = 20123;
        properties.quantized_scores = {
            {"bm25-8", {irk::index::ScoreType::BM25, 0.0, 25.1, 8}},
            {"bm25-24", {irk::index::ScoreType::BM25, 0.0, 25.1, 24}}
        };
    }
};

TEST_F(PropertiesTest, read)
{
    // given
    {
        std::ofstream os((index_dir / "properties.json").c_str());
        os << serialized;
    }

    // when
    auto deserialized = irk::index::Properties::read(index_dir);

    // then
    EXPECT_EQ(deserialized.skip_block_size, properties.skip_block_size);
    EXPECT_EQ(deserialized.occurrences_count, properties.occurrences_count);
    EXPECT_EQ(deserialized.document_count, properties.document_count);
    EXPECT_EQ(deserialized.avg_document_size, properties.avg_document_size);
    EXPECT_EQ(deserialized.max_document_size, properties.max_document_size);
    auto qs8 = deserialized.quantized_scores["bm25-8"];
    auto eqs8 = properties.quantized_scores["bm25-8"];
    EXPECT_EQ(qs8.type, eqs8.type);
    EXPECT_EQ(qs8.nbits, eqs8.nbits);
    EXPECT_EQ(qs8.min, eqs8.min);
    EXPECT_EQ(qs8.max, eqs8.max);
    auto qs24 = deserialized.quantized_scores["bm25-24"];
    auto eqs24 = properties.quantized_scores["bm25-24"];
    EXPECT_EQ(qs24.type, eqs24.type);
    EXPECT_EQ(qs24.nbits, eqs24.nbits);
    EXPECT_EQ(qs24.min, eqs24.min);
    EXPECT_EQ(qs24.max, eqs24.max);
}

TEST_F(PropertiesTest, write)
{
    // given
    auto jprop = nlohmann::json::parse(serialized);

    // when
    nlohmann::json written;
    irk::index::Properties::write(properties, index_dir);
    {
        std::ifstream is((index_dir / "properties.json").c_str());
        is >> written;
    }

    // then
    EXPECT_EQ(written["skip_block_size"], jprop["skip_block_size"]);
    EXPECT_EQ(written["occurrences"], jprop["occurrences"]);
    EXPECT_EQ(written["documents"], jprop["documents"]);
    EXPECT_EQ(written["avg_document_size"], jprop["avg_document_size"]);
    EXPECT_EQ(written["max_document_size"], jprop["max_document_size"]);
    EXPECT_EQ(
        written["quantized_scores"]["bm25-8"]["type"],
        jprop["quantized_scores"]["bm25-8"]["type"]);
    EXPECT_EQ(
        written["quantized_scores"]["bm25-8"]["bits"],
        jprop["quantized_scores"]["bm25-8"]["bits"]);
    EXPECT_EQ(
        written["quantized_scores"]["bm25-8"]["min"],
        jprop["quantized_scores"]["bm25-8"]["min"]);
    EXPECT_EQ(
        written["quantized_scores"]["bm25-8"]["max"],
        jprop["quantized_scores"]["bm25-8"]["max"]);
}

}  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
