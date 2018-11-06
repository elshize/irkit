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

#include <chrono>
#include <iostream>

#include <CLI/CLI.hpp>
#include <boost/filesystem.hpp>
#include <cppitertools/itertools.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <irkit/index.hpp>
#include <irkit/index/partition.hpp>
#include <irkit/index/source.hpp>
#include <irkit/timer.hpp>
#include <irkit/utils.hpp>
#include "cli.hpp"

using boost::filesystem::path;
using irk::index::document_t;
using std::uint32_t;
using namespace irk;

vmap<document_t, ShardId>
build_shard_map(const path& index_dir, const std::vector<std::string>& shards)
{
    auto data = inverted_index_mapped_data_source::from(index_dir).value();
    inverted_index_view index(&data);
    auto titles = index.titles();
    auto log = spdlog::get("partition");
    log->info("Building shard map");
    ShardId last_shard(shards.size() - 1);
    vmap<document_t, ShardId> map(titles.size(), last_shard);
    int mapped_documents(0);
    int missing_documents(0);
    ShardId shard_id(0);
    for (const auto& shard_file : shards) {
        log->info("Mapping shard {0}", static_cast<size_t>(shard_id));
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
    return map;
}

vmap<document_t, ShardId>
load_shard_map(const std::string& map_in)
{
    auto log = spdlog::get("partition");
    if (log) { log->info("Loading shard mapping from {}", map_in); }
    auto tab = load_compact_table<ShardId, vbyte_codec<ShardId>>(map_in);
    return vmap<document_t, ShardId>(tab.begin(), tab.end());
}

int main(int argc, char** argv)
{
    std::vector<std::string> shard_files;
    std::string output_dir;
    std::string map_out;
    std::string map_in;
    int shard_count;
    int batch_size = 100000;
    auto [app, args] = irk::cli::app(
        "Build mapping from document to shard",
        cli::index_dir_opt{});
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
    app->add_option("-o,--output", output_dir, "Output directory", false)
        ->required();
    auto map_in_option = app->add_option(
        "--map-in",
        map_in,
        "Use this mapping instead of computing from files",
        false);
    auto shard_count_option = app->add_option(
        "--shard-count", shard_count, "Number of shards", false);
    auto map_out_option = app->add_option(
        "--map-out", map_out, "Store mapping in this file", false);
    app->add_option(
        "-b,--batch-size",
        batch_size,
        "Number of terms to process in memory at a time",
        true);
    map_in_option->excludes(map_out_option);
    map_in_option->needs(shard_count_option);
    shard_count_option->needs(map_in_option);
    shard_count_option->excludes(map_out_option);
    map_out_option->excludes(map_in_option);
    map_out_option->excludes(shard_count_option);
    CLI11_PARSE(*app, argc, argv);

    auto log = spdlog::stderr_color_mt("partition");
    path dir(args->index_dir);
    irk::run_with_timer<std::chrono::milliseconds>(
        [&]() {
            auto shard_map = map_in.empty() ? build_shard_map(dir, shard_files)
                                            : load_shard_map(map_in);
            if (map_in.empty()) {
                shard_count = shard_files.size();
            }
            if (not map_out.empty()) {
                try {
                    auto tab =
                        build_compact_table<ShardId, vbyte_codec<ShardId>>(
                            shard_map.as_vector());
                    std::ofstream os(map_out);
                    tab.serialize(os);
                    log->info("Mapping written to: {}", map_out);
                } catch (std::exception& e) {
                    log->error("Error while saving the map: {}", e.what());
                }
            }
            irk::partition_index(
                dir, path(output_dir), shard_map, shard_count, batch_size);
        },
        irk::cli::log_finished{log});
    return 0;
}
