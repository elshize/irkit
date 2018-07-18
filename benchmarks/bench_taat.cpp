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
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;
using std::chrono::nanoseconds;
using std::chrono::time_point;

void print_hline()
{
    std::cout << std::string(61, '-') << std::endl;
}

void print_header()
{
    std::cout << std::setw(18) << std::left << "Phase";
    std::cout << std::setw(10) << std::right << "ms/query";
    std::cout << std::setw(15) << std::right << "ns/posting";
    std::cout << std::setw(18) << std::right << "mln postings/s";
    std::cout << std::endl;
}

void print(const std::string& label,
    nanoseconds time,
    int64_t posting_count,
    int query_count)
{
    auto ns_per_post = static_cast<double>(time.count()) / posting_count;
    auto total_ms = duration_cast<milliseconds>(time).count();
    auto ms_per_query = static_cast<double>(total_ms) / query_count;
    std::cout << std::setw(18) << std::left << label;
    std::cout << std::setw(10) << std::right << ms_per_query;
    std::cout << std::setw(15) << std::right << ns_per_post;
    std::cout << std::setw(18) << std::right << 1'000 / ns_per_post;
    std::cout << std::endl;
}

int main(int argc, char** argv)
{
    std::string index_dir, queries_path;
    bool stem = false;
    int k = 1000;
    int block_size;

    CLI::App app{"TAAT query benchmark."};
    app.add_flag("-s,--stem", stem, "Stem terems (Porter2)");
    app.add_option("-b,--block-size",
        block_size,
        "Use blocks of this size to aggregate",
        false);
    app.add_option("-k", k, "as in top-k", true);
    app.add_option("index_dir", index_dir, "Index directory", false)
        ->required();
    app.add_option(
           "queries_file", queries_path, "File containing queries", false)
        ->required();

    CLI11_PARSE(app, argc, argv);
    boost::filesystem::path dir(index_dir);
    irk::inverted_index_disk_data_source data(dir, "bm25");
    irk::inverted_index_view index(&data);

    nanoseconds total(0);
    nanoseconds fetch(0);
    nanoseconds init(0);
    nanoseconds accum(0);
    nanoseconds agg(0);

    std::int64_t posting_count = 0;
    int query_count = 0;
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

        auto start_time = std::chrono::high_resolution_clock::now();

        auto postings = irk::query_postings(index, terms);
        auto after_fetch = std::chrono::high_resolution_clock::now();

        time_point<high_resolution_clock> after_init;
        time_point<high_resolution_clock> after_acc;
        time_point<high_resolution_clock> end_time;
        if (app.count("--block-size") > 0 && block_size > 1) {
            irk::block_accumulator_vector<uint32_t> acc(
                index.collection_size(), block_size);
            after_init = std::chrono::high_resolution_clock::now();
            irk::taat(postings, acc);
            after_acc = std::chrono::high_resolution_clock::now();
            auto results = irk::aggregate_top_k<document_t, uint32_t>(acc, k);
            end_time = std::chrono::high_resolution_clock::now();
        }
        else {
            std::vector<uint32_t> acc(index.collection_size(), 0);
            after_init = std::chrono::high_resolution_clock::now();
            irk::taat(postings, acc);
            after_acc = std::chrono::high_resolution_clock::now();
            auto results = irk::aggregate_top_k<document_t, uint32_t>(acc, k);
            end_time = std::chrono::high_resolution_clock::now();
        }

        total += duration_cast<nanoseconds>(end_time - start_time);
        fetch += duration_cast<nanoseconds>(after_fetch - start_time);
        init += duration_cast<nanoseconds>(after_init - after_fetch);
        accum += duration_cast<nanoseconds>(after_acc - after_init);
        agg += duration_cast<nanoseconds>(end_time - after_acc);
        ++query_count;
    }
    print_header();
    print_hline();
    print("Fetching", fetch, posting_count, query_count);
    print("Initialization", init, posting_count, query_count);
    print("Accummulation", accum, posting_count, query_count);
    print("Aggregation", agg, posting_count, query_count);
    print_hline();
    print("Total", total, posting_count, query_count);

    return 0;
}
