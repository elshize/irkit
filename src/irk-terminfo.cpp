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

inline int32_t frequency(const irk::inverted_index_view& index,
    term_id_t id,
    std::optional<document_t> cutoff)
{
    if (cutoff.has_value()) {
        auto postings = index.postings(id);
        auto end = postings.lookup(cutoff.value());
        return end.idx();
    }
    return index.tdf(id);
}

inline int64_t occurrences(const irk::inverted_index_view& index,
    term_id_t id,
    std::optional<document_t> cutoff)
{
    if (cutoff.has_value()) {
        int32_t freq(0);
        auto postings = index.postings(id);
        auto it = postings.begin();
        auto end = postings.lookup(cutoff.value());
        for (; it != end; ++it) { freq += it->payload(); }
        return freq;
    }
    return index.tdf(id);
}

int main(int argc, char** argv)
{
    std::string index_dir(".");
    std::vector<std::string> terms;
    std::string sep("\t");
    bool stem = false;
    bool nohead = false;
    std::string remap_name;
    double frac_cutoff;
    document_t doc_cutoff = std::numeric_limits<document_t>::max();

    CLI::App app{"Print information about term and its posting list"};
    app.add_option("-d,--index-dir", index_dir, "index directory", true)
        ->check(CLI::ExistingDirectory);
    app.add_flag("-s,--stem", stem, "Stem terems (Porter2)");
    app.add_flag("--no-header", nohead, "Do not print header");
    app.add_option("--sep", sep, "Field separator", true);
    auto remap = app.add_option(
        "--remap", remap_name, "Name of remapping used for cutoff");
    auto fraccut =
        app.add_option("--frac-cutoff",
               frac_cutoff,
               "Early termination cutoff (top fraction of collection)")
            ->needs(remap)
            ->check(CLI::Range(0.0, 1.0));
    auto idcut = app.add_option("--doc-cutoff",
                        doc_cutoff,
                        "Early termination docID cutoff")
                     ->needs(remap)
                     ->excludes(fraccut);
    fraccut->excludes(idcut);
    app.add_option("terms", terms, "Term or terms", false)->required();
    CLI11_PARSE(app, argc, argv);

    boost::filesystem::path dir(index_dir);
    irk::inverted_index_mapped_data_source data(dir, "bm25");
    irk::inverted_index_view index(&data);

    if (fraccut->count() > 0u) {
        doc_cutoff = static_cast<document_t>(
            frac_cutoff * index.titles().size());
    }

    auto cutoff = doc_cutoff == std::numeric_limits<document_t>::max()
        ? std::nullopt
        : std::make_optional(doc_cutoff);

    if (not nohead) {
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
                      << frequency(index, id, cutoff) << sep
                      << occurrences(index, id, cutoff) << std::endl;
        }
        else {
            std::cout << term << sep << -1 << sep << 0 << sep << 0 << std::endl;
        }
    }
}
