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
//! \author Michal Siedlaczek
//! \copyright MIT License

#include <algorithm>
#include <sstream>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <irkit/coding/vbyte.hpp>
#include <irkit/coding/stream_vbyte.hpp>
#include <irkit/index/types.hpp>
#include <irkit/memoryview.hpp>

using irk::literals::operator""_id;

namespace {

TEST(stream_vbyte, unsigned_int)
{
    irk::stream_vbyte_codec<std::uint32_t> codec;
    std::vector<std::uint32_t> values = {
        0, 3, 7, 3, 18, 99, 123456, std::numeric_limits<std::uint32_t>::max()};
    std::vector<std::uint32_t> actual(values.size());
    std::vector<char> buffer(codec.max_encoded_size(values.size()));
    codec.encode(std::begin(values), std::end(values), std::begin(buffer));
    codec.decode(std::begin(buffer), std::begin(actual), values.size());
    ASSERT_THAT(actual, ::testing::ElementsAreArray(values));
}

TEST(stream_vbyte, signed_int)
{
    irk::stream_vbyte_codec<int> codec;
    std::vector<int> values = {
        0, 3, 7, 3, 18, 99, 123456, std::numeric_limits<int>::max()};
    std::vector<std::uint32_t> actual(values.size());
    std::vector<char> buffer(
        codec.max_encoded_size(values.size(), std::numeric_limits<int>::max()));
    codec.encode(std::begin(values), std::end(values), std::begin(buffer));
    codec.decode(std::begin(buffer), std::begin(actual), values.size());
    ASSERT_THAT(actual, ::testing::ElementsAreArray(values));
}

TEST(stream_vbyte, delta_decode)
{
    irk::stream_vbyte_codec<std::uint32_t> codec;
    std::array<char, 45> mem = {
        0, 0, 0, 0, 0, 0, 0, 0, 1, 3, 1, 1, 2, 4, 4, 6, 1, 4, 2, 3, 6, 5,
        4, 1, 1, 2, 5, 5, 1, 2, 1, 2, 7, 15, 2, 1, 1, 4, (char)192, 0,
        (char)136, (char)129, (char)128, 0, 99};
    std::array<irk::index::document_t, 30> nums;
    std::array<irk::index::document_t, 30> expected = {
        1_id, 4_id, 5_id, 6_id, 8_id, 12_id, 16_id, 22_id, 23_id, 27_id, 29_id,
        32_id, 38_id, 43_id, 47_id, 48_id, 49_id, 51_id, 56_id, 61_id, 62_id,
        64_id, 65_id, 67_id, 74_id, 89_id, 91_id, 92_id, 93_id, 97_id};
    irk::memory_view view = irk::make_memory_view(&mem[0], 45);
    codec.delta_decode(std::begin(mem), std::begin(nums), 30, 0);
    ASSERT_THAT(nums, ::testing::ElementsAreArray(expected));
}

TEST(vbyte, int)
{
    irk::vbyte_codec<int> codec;
    std::vector<std::uint8_t> values = {
        0, 3, 7, 3, 18, 99, 123, std::numeric_limits<std::uint8_t>::max()};
    std::vector<std::uint32_t> actual(values.size());
    std::vector<char> buffer(codec.max_encoded_size(values.size()));
    codec.encode(std::begin(values), std::end(values), std::begin(buffer));
    codec.decode(std::begin(buffer), std::begin(actual), values.size());
    ASSERT_THAT(actual, ::testing::ElementsAreArray(values));
}

TEST(vbyte, document)
{
    irk::vbyte_codec<irk::index::document_t> codec;
    std::vector<irk::index::document_t> values = {
        0_id, 3_id, 7_id, 3_id, 18_id, 99_id, 123_id,
        std::numeric_limits<irk::index::document_t>::max()};
    std::vector<irk::index::document_t> actual(values.size());
    std::vector<char> buffer(codec.max_encoded_size(values.size()));
    codec.encode(std::begin(values), std::end(values), std::begin(buffer));
    codec.decode(std::begin(buffer), std::begin(actual), values.size());
    ASSERT_THAT(actual, ::testing::ElementsAreArray(values));
}

}  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

