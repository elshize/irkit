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

#include <string>
#include <vector>

#include <CLI/CLI.hpp>
#include <boost/filesystem.hpp>

#include <irkit/index/assembler.hpp>
#include <irkit/index/types.hpp>
#include <irkit/io.hpp>

namespace fs = boost::filesystem;

int main(int argc, char** argv)
{
    std::string output_dir;
    int batch_size = 100'000;
    int skip_block_size = 64;
    int lexicon_block_size = 256;
    bool merge_only = false;
    std::string spam_titles;

    CLI::App app{"Build an inverted index."};
    app.add_flag("--merge-only", merge_only, "Merge already existing batches.");
    app.add_option("--batch-size,-b",
        batch_size,
        "Max number of documents to build in memory.",
        true);
    app.add_option(
        "--skip-block-size,-s",
        skip_block_size,
        "Size of skip blocks for inverted lists.",
        true);
    app.add_option("--lexicon-block-size",
        skip_block_size,
        "Size of skip blocks for inverted lists.",
        true);
    app.add_option(
        "--spam",
        spam_titles,
        "A file with a list of documents to ignore",
        false);
    app.add_option("output_dir", output_dir, "Index output directory", false)
        ->required();
    CLI11_PARSE(app, argc, argv);

    auto log = spdlog::stderr_color_mt("buildindex");
    if (merge_only)
    {
        fs::path dir(output_dir);
        fs::path batch_dir = dir / ".batches";
        std::vector<fs::path> batch_dirs{
            fs::directory_iterator(batch_dir), fs::directory_iterator()};
        irk::index_merger merger(dir, batch_dirs, skip_block_size);
        merger.merge();
        auto term_map = irk::build_lexicon(
            irk::index::terms_path(output_dir), lexicon_block_size);
        term_map.serialize(irk::index::term_map_path(output_dir));
        auto title_map = irk::build_lexicon(
            irk::index::titles_path(output_dir), lexicon_block_size);
        title_map.serialize(irk::index::title_map_path(output_dir));
    }
    else
    {
        std::optional<std::unordered_set<std::string>> spamlist = std::nullopt;
        if (not spam_titles.empty()) {
            spamlist = std::make_optional<std::unordered_set<std::string>>();
            for (const std::string& line : irk::io::lines(spam_titles)) {
                spamlist->insert(line);
            }
        }
        irk::index::index_assembler assembler(
            fs::path(output_dir),
            batch_size,
            skip_block_size,
            lexicon_block_size,
            spamlist);
        assembler.assemble(std::cin);
    }
    return 0;
}
