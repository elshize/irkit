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

inline std::string ExistingDirectory(const std::string &filename)
{
    struct stat buffer{};
    bool exist = stat(filename.c_str(), &buffer) == 0;
    bool is_dir = (buffer.st_mode & S_IFDIR) != 0;  // NOLINT
    if (not exist) { return "Directory does not exist: " + filename; }
    if (not is_dir) { return "Directory is actually a file: " + filename; }
    return std::string();
}

template<class Iter, class Table>
inline void
prune(Iter begin, Iter end, const Table& doc2rank, document_t doc_cutoff)
{
    document_t doc = 0;
    for (Iter it = begin; it != end; ++it) {
        if (doc2rank[doc++] > doc_cutoff) { *it = 0; }
    }
}

template<class Index>
inline void run_query(const Index& index,
    const boost::filesystem::path& dir,
    std::vector<std::string>& query,
    int k,
    bool stem,
    const std::string& remap_name,
    std::optional<document_t> cutoff,
    std::optional<int> trecid)
{
    if (stem) {
        irk::porter2_stemmer stemmer;
        for (auto& term : query) {
            term = stemmer.stem(term);
        }
    }

    auto start_time = std::chrono::steady_clock::now();

    auto postings = irk::query_postings(index, query);
    auto after_fetch = std::chrono::steady_clock::now();

    std::vector<uint32_t> acc(index.collection_size(), 0);
    auto after_init = std::chrono::steady_clock::now();

    irk::taat(postings, acc);
    auto after_acc = std::chrono::steady_clock::now();

    if (cutoff.has_value()) {
        auto doc2rank = irk::load_compact_table<document_t>(
            dir / (remap_name + ".doc2rank"));
        prune(std::begin(acc), std::end(acc), doc2rank, cutoff.value());
    }

    auto results = irk::aggregate_top_k<document_t, uint32_t>(acc, k);
    auto end_time = std::chrono::steady_clock::now();

    auto total = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    auto fetch = std::chrono::duration_cast<std::chrono::milliseconds>(
        after_fetch - start_time);
    auto init = std::chrono::duration_cast<std::chrono::milliseconds>(
        after_init - after_fetch);
    auto accum = std::chrono::duration_cast<std::chrono::milliseconds>(
        after_acc - after_init);
    auto agg = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - after_acc);

    const auto& titles = index.titles();
    int rank = 0;
    for (auto& result : results)
    {
        if (trecid.has_value()) {
            std::cout << *trecid << '\t'
                      << "Q0\t"
                      << titles.key_at(result.first) << "\t"
                      << rank++ << "\t"
                      << result.second << "\tnull\n";
        }
        else {
            std::cout << titles.key_at(result.first) << "\t" << result.second
                      << '\n';
        }
    }
    std::cerr << "Time: " << total.count() << " ms [ ";
    std::cerr << "Fetch: " << fetch.count() << " / ";
    std::cerr << "Init: " << init.count() << " / ";
    std::cerr << "Acc: " << accum.count() << " / ";
    std::cerr << "Agg: " << agg.count() << " ]\n";
}

int main(int argc, char** argv)
{
    std::string index_dir = ".";
    std::vector<std::string> query;
    int k = 1000;
    bool stem = false;
    int trecid = -1;
    std::string remap_name;
    double frac_cutoff;
    document_t doc_cutoff;

    CLI::App app{"Query inverted index"};
    app.add_option("-d,--index-dir", index_dir, "index directory", true)
        ->check(CLI::ExistingDirectory);
    app.add_option("-k", k, "as in top-k", true);
    app.add_flag("-s,--stem", stem, "Stem terems (Porter2)");
    app.add_flag("-f,--file", stem, "Read queries from file(s)");
    app.add_option(
        "--trecid", trecid, "Print in trec_eval format with this QID");
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
    app.add_option("query", query, "Query (or query file with -f)", false)
        ->required();
    CLI11_PARSE(app, argc, argv);

    std::cerr << "Loading index..." << std::flush;
    boost::filesystem::path dir(index_dir);
    irk::inverted_index_mapped_data_source data(dir, "bm25");
    irk::inverted_index_view index(&data);
    std::cerr << " done." << std::endl;

    if (fraccut->count() > 0u) {
        doc_cutoff = static_cast<document_t>(
            frac_cutoff * index.titles().size());
    }

    if (app.count("--file") == 0u) {
        run_query(index,
            dir,
            query,
            k,
            stem,
            remap_name,
            fraccut->count() + idcut->count() > 0u
                ? std::make_optional(doc_cutoff)
                : std::nullopt,
            app.count("--trecid") > 0u ? std::make_optional(trecid)
                                       : std::nullopt);
    }
    else {
        std::optional<int> current_trecid = app.count("--trecid") > 0u
            ? std::make_optional(trecid)
            : std::nullopt;
        for (const auto& file : query) {
            std::ifstream in(file);
            std::string q;
            while(std::getline(in, q))
            {
                std::istringstream qin(q);
                std::string term;
                std::vector<std::string> terms;
                while (qin >> term) { terms.push_back(std::move(term)); }
                run_query(index,
                    dir,
                    terms,
                    k,
                    stem,
                    remap_name,
                    fraccut->count() + idcut->count() > 0u
                        ? std::make_optional(doc_cutoff)
                        : std::nullopt,
                    current_trecid);
                if (current_trecid.has_value()) { current_trecid.value()++; }
            }
        }
    }
}
