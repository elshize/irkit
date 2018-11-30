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

#include <iostream>

#include <CLI/CLI.hpp>
#include <boost/filesystem.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <irkit/coding/hutucker.hpp>
#include <irkit/compacttable.hpp>
#include <irkit/index.hpp>
#include <irkit/index/source.hpp>
#include <irkit/io.hpp>
#include <irkit/lexicon.hpp>
#include <irkit/memoryview.hpp>
#include "cli.hpp"

using boost::filesystem::path;
using irk::index::document_t;
using std::uint32_t;

void build_shard_map(
    const std::vector<std::string>& shards,
    const irk::lexicon<irk::hutucker_codec<char>, irk::memory_view>& titles,
    const path& output_file)
{
    auto log = spdlog::stderr_color_mt("stderr");
    log->info("Building shard map");
    auto last_shard = shards.size() - 1;
    std::vector<uint32_t> map(titles.size(), last_shard);
    int mapped_documents(0);
    int missing_documents(0);
    int shard_id(0);
    for (const auto& shard_file : shards) {
        log->info("Mapping shard {0}", shard_id);
        for (const std::string& title : irk::io::lines(shard_file)) {
            if (auto id = titles.index_at(title); id.has_value()) {
                map[id.value()] = shard_id;
                mapped_documents += 1;
            } else {
                missing_documents += 1;
            }
        }
        shard_id += 1;
    }
    log->info(
        "Mapped {}; missing in index: {}; defaulted to last: {}",
        mapped_documents,
        missing_documents,
        titles.size() - mapped_documents);
    log->info("Writing shard map");
    auto table = irk::build_compact_table<uint32_t>(map);
    irk::io::dump(table, output_file);
}

int main(int argc, char** argv)
{
    std::string mapping_name;
    std::vector<std::string> shard_files;

    auto [app, args] = irk::cli::app(
        "Build mapping from document to shard",
        irk::cli::index_dir_opt{});
    app->add_option(
           "-n,--name",
           mapping_name,
           "Name of the mapping",
           false)
        ->required();
    app->add_option(
           "shards",
           shard_files,
           "Files describing shards:\t"
           "The files must contain a whitespace-delimited list of TREC IDs. "
           "The documents in the first file will be assigned to shard 0, "
           "the second file to shard 1, and so on. "
           "If a given document repeats, it will be overwritten. "
           "Documents that don't exist in the index will be ignored. "
           "Documents in the index not present in any input file will be "
           "appended to the last shard.",
           false)
        ->required();
    CLI11_PARSE(*app, argc, argv);

    path dir(args->index_dir);
    irk::inverted_index_view index(irtl::value(irk::Inverted_Index_Mapped_Source::from(dir)));
    build_shard_map(shard_files, index.titles(), dir / (mapping_name + ".shardmap"));
    return 0;
}
