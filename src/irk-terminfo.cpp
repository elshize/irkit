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
#include <CLI/Option.hpp>
#include <boost/filesystem.hpp>

#include <irkit/compacttable.hpp>
#include <irkit/index.hpp>
#include <irkit/index/source.hpp>
#include <irkit/parsing/stemmer.hpp>
#include <irkit/taat.hpp>
#include "cli.hpp"

using std::uint32_t;
using irk::index::document_t;
using irk::index::term_id_t;

inline std::string ExistingDirectory(const std::string &filename)
{
    struct stat buffer{};
    bool exist = stat(filename.c_str(), &buffer) == 0;
    bool is_dir = (buffer.st_mode & S_IFDIR) != 0;  // NOLINT
    if (not exist) { return "Directory does not exist: " + filename; }
    if (not is_dir) { return "Directory is actually a file: " + filename; }
    return std::string();
}

std::pair<document_t, document_t> to_ids(
    const std::vector<double>& range,
    const irk::inverted_index_view& index)
{
    auto size = index.collection_size();
    return std::make_pair(
        static_cast<document_t>(range[0] * size),
        static_cast<document_t>(range[1] * size));
}

inline int32_t frequency(
    const irk::inverted_index_view& index,
    term_id_t id,
    std::vector<double> id_range)
{
    if (id_range == std::vector{0.0, 1.0}) { return index.tdf(id); }
    auto [first_id, last_id] = to_ids(id_range, index);
    auto postings = index.postings(id);
    auto begin = postings.lookup(first_id);
    auto end = begin.nextgeq(last_id);
    return end.idx() - begin.idx();
}

inline int64_t occurrences(
    const irk::inverted_index_view& index,
    term_id_t id,
    std::vector<double> id_range)
{
    int32_t freq(0);
    auto postings = index.postings(id);
    auto [first_id, last_id] = to_ids(id_range, index);
    auto it = postings.lookup(first_id);
    auto end = it.nextgeq(last_id);
    for (; it != end; ++it) { freq += it->payload(); }
    return freq;
}

int main(int argc, char** argv)
{
    std::vector<std::string> terms;
    auto [app, args] = irk::cli::app(
        "Print information about term and its posting list",
        irk::cli::index_dir_opt{},
        irk::cli::nostem_opt{},
        irk::cli::noheader_opt{},
        irk::cli::sep_opt{},
        irk::cli::id_range_opt{});
    app->add_option("terms", terms, "Term(s)", false)->required();
    CLI11_PARSE(*app, argc, argv);
    boost::filesystem::path dir(args->index_dir);
    irk::inverted_index_mapped_data_source data(dir);
    irk::inverted_index_view index(&data);

    const std::string sep = args->separator;
    if (not args->noheader) {
        std::cout
            << "term" << sep
            << "id" << sep
            << "frequency" << sep
            << "occurrences" << std::endl;
    }

    for (const std::string& term : terms) {
        auto term_id = index.term_id(term);
        if (term_id.has_value()) {
            auto id = term_id.value();
            std::cout << term << sep << id << sep
                      << frequency(index, id, args->id_range) << sep
                      << occurrences(index, id, args->id_range) << std::endl;
        }
        else {
            std::cout << term << sep << -1 << sep << 0 << sep << 0 << std::endl;
        }
    }
}
