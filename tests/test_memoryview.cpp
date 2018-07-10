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

//! \file test_memoryview.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#include <algorithm>
#include <gsl/span>
#include <random>
#include <sstream>
#include <vector>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#define private public
#define protected public
#include <gsl/span>
#include <irkit/memoryview.hpp>

namespace {

class span_memory_source : public ::testing::Test {
protected:
    std::vector<char> container = {4, 2, 1, 4, 6};
    irk::memory_view view = irk::make_memory_view(container);
};

void test_size(const irk::memory_view& view, const std::vector<char>& container)
{
    ASSERT_EQ(view.size(), container.size());
}

void test_iterator(
    const irk::memory_view& view, const std::vector<char>& container)
{
    std::vector<char> from_iterator(view.begin(), view.end());
    EXPECT_THAT(from_iterator, ::testing::ElementsAreArray(container));
}

void test_slice(const irk::memory_view& view,
    const std::vector<char>& container,
    irk::memory_view::slice_type slice)
{
        auto subview = view[slice];
        std::vector<char> from_iterator(subview.begin(), subview.end());
        EXPECT_THAT(from_iterator, ::testing::ElementsAreArray(container));
}

void test_slices(const irk::memory_view& view,
    const std::vector<char>& container)
{
    test_slice(view, container, {std::nullopt, std::nullopt});
    test_slice(view,
        std::vector<char>(container.begin() + 1, container.end()),
        {1, std::nullopt});
    test_slice(view,
        std::vector<char>(container.begin() + 1, container.end()),
        {1, container.size() - 1});
    test_slice(view,
        std::vector<char>(container.begin(), container.end() - 2),
        {std::nullopt, container.size() - 3});

    auto s = view.range(1, 3);
    test_slice(s, {2, 1, 4}, {std::nullopt, std::nullopt});
    test_slice(s, {1, 4}, {1, std::nullopt});
    test_slice(s, {2, 1}, {std::nullopt, 1});
    test_slice(s, {1}, {1, 1});
    auto ss = s.range(1, 2);
    test_slice(ss, {1, 4}, {std::nullopt, std::nullopt});
    test_slice(ss, {4}, {1, std::nullopt});
}

TEST_F(span_memory_source, size)
{
    test_size(view, container);
}

TEST_F(span_memory_source, iterator)
{
    test_iterator(view, container);
}

TEST_F(span_memory_source, slice)
{
    test_slices(view, container);
}

class disk_memory_source : public ::testing::Test {
protected:
    std::vector<char> container = {4, 2, 1, 4, 6};
    boost::filesystem::path path;
    irk::memory_view view;

    disk_memory_source() {
        auto tmpdir = boost::filesystem::temp_directory_path();
        auto dir = tmpdir / "irkit-disk_memory_source";
        if (boost::filesystem::exists(dir)) {
            boost::filesystem::remove_all(dir);
        }
        boost::filesystem::create_directory(dir);
        path = dir / "source_file";
        std::ofstream out(path.c_str());
        out.write(&container[0], container.size());
        out.flush();
        view = irk::make_memory_view(path);
    }
};

TEST_F(disk_memory_source, size)
{
    test_size(view, container);
}

TEST_F(disk_memory_source, iterator)
{
    test_iterator(view, container);
}

TEST_F(disk_memory_source, slice)
{
    test_slices(view, container);
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
