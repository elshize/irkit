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
#include <boost/log/trivial.hpp>

#include <irkit/index.hpp>
#include <irkit/index/source.hpp>
#include <irkit/compacttable.hpp>

using boost::filesystem::path;
using irk::index::document_t;
using std::uint32_t;

inline std::string ExistingDirectory(const std::string &filename) {
    struct stat buffer{};
    bool exist = stat(filename.c_str(), &buffer) == 0;
    bool is_dir = (buffer.st_mode & S_IFDIR) != 0;  // NOLINT
    if (not exist) { return "Directory does not exist: " + filename; }
    if (not is_dir) { return "Directory is actually a file: " + filename; }
    return std::string();
}

int main(int argc, char** argv)
{
    std::string index_dir = ".";
    std::string ordering_file;
    std::string mapping_name;

    CLI::App app{"Build mapping from document IDs to their static rank"};
    app.add_option("-d,--index-dir", index_dir, "index directory", true)
        ->check(CLI::ExistingDirectory);
    app.add_option("-n,--name",
        mapping_name,
        "mapping name to use instead of ordering file name",
        false);
    app.add_option("ordering", ordering_file, "ordering file", false)
        ->required();
    CLI11_PARSE(app, argc, argv);

    if (app.count("--name") == 0u) { mapping_name = ordering_file; }

    BOOST_LOG_TRIVIAL(info) << "Loading index...";
    boost::filesystem::path dir(index_dir);
    irk::inverted_index_mapped_data_source data(dir);
    irk::inverted_index_view index(&data);
    const auto& title_map = index.titles();

    BOOST_LOG_TRIVIAL(info) << "Computing mappings...";
    std::vector<document_t> doc2rank(
        title_map.size(), static_cast<document_t>(title_map.size()));
    std::vector<document_t> rank2doc(title_map.size());
    document_t rank = 0;
    std::ifstream ord(ordering_file);
    std::string title;
    while (std::getline(ord, title))
    {
        auto docid = title_map.index_at(title);
        if (docid.has_value()) {
            doc2rank[*docid] = rank;
            rank2doc[rank] = *docid;
            rank++;
        }
    }
    for (document_t docid = 0; docid < doc2rank.size(); ++docid) {
        if (doc2rank[docid] == static_cast<document_t>(title_map.size())) {
            doc2rank[docid] = rank;
            rank2doc[rank] = docid;
            rank++;
        }
    }

    BOOST_LOG_TRIVIAL(info) << "Compacting...";
    auto doc2rank_table = irk::build_compact_table(doc2rank);
    auto rank2doc_table = irk::build_compact_table(rank2doc);

    BOOST_LOG_TRIVIAL(info) << "Writing...";
    irk::io::dump(
        doc2rank_table, path(index_dir) / (mapping_name + ".doc2rank"));
    irk::io::dump(
        rank2doc_table, path(index_dir) / (mapping_name + ".rank2doc"));
    BOOST_LOG_TRIVIAL(info) << "Done.";
}
