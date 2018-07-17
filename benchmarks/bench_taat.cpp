// MIT License
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

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>
#include <boost/filesystem.hpp>

#include <irkit/index.hpp>
#include <irkit/index/source.hpp>
#include <irkit/index/types.hpp>
#include <irkit/parsing/stemmer.hpp>
#include <irkit/taat.hpp>

using irk::index::document_t;
using std::chrono::milliseconds;
using std::chrono::duration_cast;

int main(int argc, char** argv)
{
    std::string index_dir, queries_path;
    bool stem = false;
    int k = 1000;

    CLI::App app{"TAAT query benchmark."};
    app.add_option("index_dir", index_dir, "Index directory", false)
        ->required();
    app.add_flag("-s,--stem", stem, "Stem terems (Porter2)");
    app.add_option("-k", k, "as in top-k", true);
    app.add_option(
           "queries_file", queries_path, "File containing queries", false)
        ->required();

    CLI11_PARSE(app, argc, argv);
    std::cout << "Loading index...";
    boost::filesystem::path dir(index_dir);
    irk::inverted_index_disk_data_source data(dir, "bm25");
    irk::inverted_index_view index(&data);
    std::cout << " done." << std::endl;

    milliseconds total(0);
    milliseconds fetch(0);
    milliseconds init(0);
    milliseconds accum(0);
    milliseconds agg(0);

    int posting_count = 0;
    std::string query;
    std::ifstream in(queries_path);
    while (std::getline(in, query)) {
        std::string term;
        std::vector<std::string> terms;
        std::istringstream line(query);
        while (line >> term)
        {
            if (stem)
            {
                irk::porter2_stemmer stemmer;
                terms.push_back(stemmer.stem(term));
            }
            else { terms.push_back(std::move(term)); }
        }
        auto query_postings = irk::query_postings(index, terms);
        for (const auto& posting_list : query_postings) {
            posting_count += posting_list.size();
        }

        auto start_time = std::chrono::steady_clock::now();

        auto postings = irk::query_postings(index, terms);
        auto after_fetch = std::chrono::steady_clock::now();

        std::vector<uint32_t> acc(index.collection_size(), 0);
        auto after_init = std::chrono::steady_clock::now();

        irk::taat(postings, acc);
        auto after_acc = std::chrono::steady_clock::now();

        auto results = irk::aggregate_top_k<document_t, uint32_t>(acc, k);
        auto end_time = std::chrono::steady_clock::now();

        total += duration_cast<milliseconds>(end_time - start_time);
        fetch += duration_cast<milliseconds>(after_fetch - start_time);
        init += duration_cast<milliseconds>(after_init - after_fetch);
        accum += duration_cast<milliseconds>(after_acc - after_init);
        agg += duration_cast<milliseconds>(end_time - after_acc);
        posting_count++;
    }
    auto total_ns_per_post = static_cast<double>(total.count()) / posting_count;
    auto fetch_ns_per_post = static_cast<double>(fetch.count()) / posting_count;
    auto init_ns_per_post = static_cast<double>(fetch.count()) / posting_count;
    auto accum_ns_per_post = static_cast<double>(fetch.count()) / posting_count;
    auto agg_ns_per_post = static_cast<double>(fetch.count()) / posting_count;

    std::cout << "Total: " << total_ns_per_post << " ns/p\t";
    std::cout << 1'000 / total_ns_per_post << " mln p/s" << std::endl;

    std::cout << "Fetch: " << fetch_ns_per_post << " ns/p\t";
    std::cout << 1'000 / fetch_ns_per_post << " mln p/s" << std::endl;

    std::cout << "Init: " << init_ns_per_post << " ns/p\t";
    std::cout << 1'000 / init_ns_per_post << " mln p/s" << std::endl;

    std::cout << "Accumulation: " << accum_ns_per_post << " ns/p\t";
    std::cout << 1'000 / accum_ns_per_post << " mln p/s" << std::endl;

    std::cout << "Aggregation: " << agg_ns_per_post << " ns/p\t";
    std::cout << 1'000 / agg_ns_per_post << " mln p/s" << std::endl;

    return 0;
}
