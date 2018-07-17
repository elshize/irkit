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

template<class T>
inline void do_not_optimize_away(T&& datum)
{
    asm volatile("" : "+r"(datum));  // NOLINT
}

int main(int argc, char** argv)
{
    std::string index_dir, term;

    CLI::App app{"Posting reading benchmark."};
    app.add_option("index_dir", index_dir, "Index directory", false)
        ->required();
    app.add_option("term", term, "Term", false)->required();

    CLI11_PARSE(app, argc, argv);
    std::cout << "Loading index...";
    boost::filesystem::path dir(index_dir);
    irk::inverted_index_disk_data_source data(dir);
    irk::inverted_index_view index(&data);
    std::cout << " done." << std::endl;

    std::chrono::nanoseconds together_elapsed(0);

    auto term_id = index.term_id(term).value_or(0);
    {
        std::chrono::nanoseconds elapsed(0);
        auto document_list = index.documents(term_id);
        auto last = document_list.end();
        auto docit = document_list.begin();
        auto start = std::chrono::steady_clock::now();
        for (; docit != last; ++docit) {
            irk::index::document_t d = *docit;
            asm volatile("" : "+r"(d));  // NOLINT
        }
        auto end = std::chrono::steady_clock::now();
        elapsed += std::chrono::duration_cast<std::chrono::nanoseconds>(
            end - start);
        auto ns_per_int = static_cast<double>(elapsed.count())
            / document_list.size();
        std::cout << "Documents only: " << ns_per_int
                  << " ns/p; " << 1'000 / ns_per_int << " mln p/s" << std::endl;
    }

    {
        std::chrono::nanoseconds elapsed(0);
        auto document_list = index.documents(term_id);
        auto frequency_list = index.frequencies(term_id);
        auto last = document_list.end();
        auto docit = document_list.begin();
        auto freqit = frequency_list.begin();
        auto start = std::chrono::steady_clock::now();
        for (; docit != last; ++docit, ++freqit) {
            irk::index::document_t d = *docit;
            irk::index::document_t f = *freqit;
            asm volatile("" : "+r"(d));  // NOLINT
            asm volatile("" : "+r"(f));  // NOLINT
        }
        auto end = std::chrono::steady_clock::now();
        elapsed += std::chrono::duration_cast<std::chrono::nanoseconds>(
            end - start);
        auto ns_per_int = static_cast<double>(elapsed.count())
            / document_list.size();
        std::cout << "Documents and frequencies independently: " << ns_per_int
                  << " ns/p; " << 1'000 / ns_per_int << " mln p/s" << std::endl;
    }

    {
        auto posting_list = index.postings(term_id);
        auto it = posting_list.begin();
        auto last = posting_list.end();
        auto start = std::chrono::steady_clock::now();
        for (auto p : posting_list) {
            irk::index::document_t d = p.document();
            irk::index::document_t f = p.payload();
            asm volatile("" : "+r"(d));  // NOLINT
            asm volatile("" : "+r"(f));  // NOLINT
        }
        auto end = std::chrono::steady_clock::now();
        together_elapsed +=
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        auto ns_per_int = static_cast<double>(together_elapsed.count())
            / posting_list.size();
        std::cout << "As posting_list_view: " << ns_per_int
                  << " ns/p; " << 1'000 / ns_per_int << " mln p/s" << std::endl;
    }

    return 0;
}
