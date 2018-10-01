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

#include "cli.hpp"
#include <irkit/compacttable.hpp>
#include <irkit/index.hpp>
#include <irkit/index/source.hpp>
#include <irkit/parsing/stemmer.hpp>
#include <irkit/taat.hpp>

using std::uint32_t;
using irk::index::document_t;
using mapping_t = std::vector<document_t>;

template<class Index>
inline void run_query(const Index& index,
    const boost::filesystem::path& dir,
    std::vector<std::string>& query,
    int k,
    bool stem,
    irk::cli::docmap* reordering,
    std::optional<document_t> cutoff,
    std::optional<int> trecid,
    std::string_view run_id)
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

    if (reordering == nullptr) { irk::taat(postings, acc); }
    else { irk::taat(postings, acc, reordering->doc2rank()); }

    auto after_acc = std::chrono::steady_clock::now();

    auto results = cutoff.has_value()
        ? irk::aggregate_top_k<document_t, uint32_t>(
              std::begin(acc), std::next(std::begin(acc), *cutoff), k)
        : irk::aggregate_top_k<document_t, uint32_t>(
              std::begin(acc), std::end(acc), k);
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
        std::string title = titles.key_at(reordering != nullptr
                ? reordering->doc(result.first)
                : result.first);
        if (trecid.has_value()) {
            std::cout << *trecid << '\t'
                      << "Q0\t"
                      << title << "\t"
                      << rank++ << "\t"
                      << result.second << "\t"
                      << run_id << "\n";
        }
        else {
            std::cout << title << "\t" << result.second << '\n';
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
    auto [app, args] = irk::cli::app("Query inverted index",
        irk::cli::index_dir_opt{},
        irk::cli::query_opt{},
        irk::cli::reordering_opt{},
        irk::cli::metric_opt{},
        irk::cli::etcutoff_opt{});
    CLI11_PARSE(*app, argc, argv);

    boost::filesystem::path dir(args->index_dir);
    auto data =
        irk::inverted_index_mapped_data_source::from(dir, {"bm25"}).value();
    irk::inverted_index_view index(&data);

    std::unique_ptr<irk::cli::docmap> reordering = nullptr;
    if (not args->reordering.empty()) {
        *reordering = irk::cli::docmap::from_files(args->reordering);
    }

    if (app->count("--frac-cutoff") > 0u) {
        args->doc_cutoff = static_cast<document_t>(
            args->frac_cutoff * index.titles().size());
    }

    if (not args->read_files) {
        run_query(index,
            args->index_dir,
            args->terms_or_files,
            args->k,
            not args->nostem,
            reordering.get(),
            app->count("--frac-cutoff") + app->count("--doc-cutoff") > 0u
                ? std::make_optional(args->doc_cutoff)
                : std::nullopt,
            args->trecid != -1 ? std::make_optional(args->trecid)
                               : std::nullopt,
            args->trecrun);
    }
    else {
        std::optional<int> current_trecid = app->count("--trecid") > 0u
            ? std::make_optional(args->trecid)
            : std::nullopt;
        for (const auto& file : args->terms_or_files) {
            std::ifstream in(file);
            std::string q;
            while(std::getline(in, q))
            {
                std::istringstream qin(q);
                std::string term;
                std::vector<std::string> terms;
                while (qin >> term) { terms.push_back(std::move(term)); }
                run_query(index,
                    args->index_dir,
                    args->terms_or_files,
                    args->k,
                    not args->nostem,
                    reordering.get(),
                    app->count("--frac-cutoff") + app->count("--doc-cutoff")
                            > 0u
                        ? std::make_optional(args->doc_cutoff)
                        : std::nullopt,
                    current_trecid,
                    args->trecrun);
                if (current_trecid.has_value()) { current_trecid.value()++; }
            }
        }
    }
}
