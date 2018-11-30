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

#pragma once

#include <boost/filesystem.hpp>

#include <irkit/index/assembler.hpp>
#include <irkit/index/score.hpp>
#include <irkit/index/scoreable_index.hpp>

namespace irk::test {

auto tmpdir()
{
    auto test_dir = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
    if (boost::filesystem::exists(test_dir)) {
        boost::filesystem::remove(test_dir);
    }
    boost::filesystem::create_directory(test_dir);
    return test_dir;
}

void build_test_index(const boost::filesystem::path& index_dir,
                      bool score = true,
                      bool calc_stats = true)
{
    irk::index::index_assembler assembler(index_dir, 100, 4, 16);
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
        "dignissim felis gravida.\n"
        "Doc09 Cras pulvinar ante in massa euismod tempor.\n");
    assembler.assemble(input);

    if (score) {
        irk::index::score_index<irk::score::bm25_tag, irk::Inverted_Index_Mapped_Source>(index_dir,
                                                                                         8);
    }
    if (calc_stats) {
        irk::Scoreable_Index::from(index_dir, "bm25").calc_score_stats();
    }
}

}  // namespace irk::test
