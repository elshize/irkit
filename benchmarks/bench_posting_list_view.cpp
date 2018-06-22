// MIT License //
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

#include <boost/filesystem.hpp>
#include <CLI/CLI.hpp>

#include <irkit/coding/varbyte.hpp>
#include <irkit/index.hpp>
#include <irkit/index/source.hpp>

int main(int argc, char** argv)
{
    std::string index_dir;
    int time_limit;
    double overhead_limit;

    CLI::App app{"Posting reading benchmark."};
    app.add_option(
           "--time-limit", time_limit, "Time limit per posting in ns", false)
        ->required();
    app.add_option("--overhead-limit",
           overhead_limit,
           "Zipped posting list overhead limit per posting (e.g., 1.1 means "
           "max 110\% of sequential time",
           false)
        ->required();
    app.add_option("index_dir", index_dir, "Index directory", false)
        ->required();

    CLI11_PARSE(app, argc, argv);
    std::cout << "Loading index...";
    boost::filesystem::path dir(index_dir);
    irk::inverted_index_inmemory_data_source data(dir);
    irk::inverted_index_view index(&data,
        irk::varbyte_codec<long>{},
        irk::varbyte_codec<long>{},
        irk::varbyte_codec<long>{});
    std::cout << " done." << std::endl;

    std::chrono::nanoseconds independent_elapsed(0);
    std::chrono::nanoseconds together_elapsed(0);

    long posting_count = 0;
    for (long term_id = 0; term_id < index.terms().size(); term_id++) {
        auto document_list = index.documents(term_id);
        auto frequency_list = index.frequencies(term_id);
        if (document_list.size() < 1000) continue;
        auto last = document_list.end();
        auto docit = document_list.begin();
        auto freqit = frequency_list.begin();
        auto start = std::chrono::steady_clock::now();
        for (; docit != last; ++docit, ++freqit) {
            ++posting_count;
        }
        auto end = std::chrono::steady_clock::now();
        independent_elapsed +=
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    }
    int independent_avg = independent_elapsed.count() / posting_count;
    std::cout << "Documents and frequencies independently: " << independent_avg
              << "ns/posting" << std::endl;

    posting_count = 0;
    for (long term_id = 0; term_id < index.terms().size(); term_id++) {
        auto posting_list = index.postings(term_id);
        if (posting_list.size() < 1000) continue;
        auto it = posting_list.begin();
        auto last = posting_list.end();
        auto start = std::chrono::steady_clock::now();
        for (; it != last; ++it) { ++posting_count; }
        auto end = std::chrono::steady_clock::now();
        together_elapsed +=
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    }
    int together_avg = together_elapsed.count() / posting_count;
    std::cout << "As posting_list_view: " << together_avg << "ns/posting"
              << std::endl;

    assert(independent_avg <= time_limit);
    assert(together_avg <= time_limit);
    assert((double)together_avg <= overhead_limit * independent_avg);
    return 0;
}

