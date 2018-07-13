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

#include <irkit/index.hpp>
#include <irkit/index/source.hpp>
#include <irkit/taat.hpp>

using std::uint32_t;
using irk::index::document_t;

auto query_postings(const irk::inverted_index_view& index,
    const std::vector<std::string>& query)
{
    using posting_list_type = decltype(
        index.scored_postings(std::declval<std::string>()));
    std::vector<posting_list_type> postings;
    postings.reserve(query.size());
    for (const auto& term : query)
    { postings.push_back(index.scored_postings(term)); }
    return postings;
}

int main(int argc, char** argv)
{
    std::string index_dir = ".";
    std::vector<std::string> query;
    int k = 1000;

    CLI::App app{"Query inverted index"};
    app.add_option("-d,--index-dir", index_dir, "index directory", true)
        ->check(CLI::ExistingDirectory);
    app.add_option("-k", k, "as in top-k", true);
    app.add_option("query", query, "Query", false)->required();

    CLI11_PARSE(app, argc, argv);

    std::cout << "Loading index...";
    boost::filesystem::path dir(index_dir);
    irk::inverted_index_disk_data_source data(dir, "bm25");
    irk::inverted_index_view index(&data);
    std::cout << " done." << std::endl;

    auto start_time = std::chrono::steady_clock::now();

    std::vector<uint32_t> acc(index.collection_size());
    irk::taat(query_postings(index, query), acc);
    auto results = irk::aggregate_top_k<document_t, uint32_t>(acc, k);

    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    const auto& titles = index.titles();
    for (auto& result : results)
    {
        std::cout << titles.key_at(result.first) << "\t" << result.second
                  << std::endl;
    }
    std::cout << "Query time: " << elapsed.count() << " ms" << std::endl;
}
