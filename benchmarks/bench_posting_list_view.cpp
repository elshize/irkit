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
#include <irkit/timer.hpp>

template<class T>
inline void do_not_optimize_away(T&& datum)
{
    asm volatile("" : "+r"(datum));  // NOLINT
}

auto print(const std::string& label, size_t count)
{
    return [=](std::chrono::nanoseconds elapsed) {
        auto ns_per_int = static_cast<double>(elapsed.count()) / count;
        std::cout << label << ": " << ns_per_int << " ns/p; "
                  << 1'000 / ns_per_int << " mln p/s\n";
    };
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
    auto data = irk::inverted_index_mapped_data_source::from(dir);
    irk::inverted_index_view index(&data.value());
    std::cout << " done." << std::endl;

    auto term_id = index.term_id(term).value_or(0);
    auto count = index.term_collection_frequency(term_id);

    irk::run_with_timer<std::chrono::nanoseconds>(
        [&] {
            auto document_list = index.documents(term_id);
            auto last = document_list.end();
            auto docit = document_list.begin();
            for (; docit != last; ++docit) {
                irk::index::document_t d = *docit;
                asm volatile("" : "+r"(d));  // NOLINT
            }
            return document_list.size();
        },
        print("Documents only", count));

    irk::run_with_timer<std::chrono::nanoseconds>(
        [&] {
            auto frequency_list = index.frequencies(term_id);
            auto last = frequency_list.end();
            auto docit = frequency_list.begin();
            for (; docit != last; ++docit) {
                auto d = *docit;
                asm volatile("" : "+r"(d));  // NOLINT
            }
        },
        print("Frequencies only", count));

    irk::run_with_timer<std::chrono::nanoseconds>(
        [&] {
            auto document_list = index.documents(term_id);
            auto frequency_list = index.frequencies(term_id);
            auto last = document_list.end();
            auto docit = document_list.begin();
            auto freqit = frequency_list.begin();
            for (; docit != last; ++docit, ++freqit) {
                irk::index::document_t d = *docit;
                irk::index::frequency_t f = *freqit;
                asm volatile("" : "+r"(d));  // NOLINT
                asm volatile("" : "+r"(f));  // NOLINT
            }
        },
        print("Documents and frequencies independently", count));

    irk::run_with_timer<std::chrono::nanoseconds>(
        [&] {
            auto posting_list = index.postings(term_id);
            for (auto p : posting_list) {
                irk::index::document_t d = p.document();
                irk::index::frequency_t f = p.payload();
                asm volatile("" : "+r"(d));  // NOLINT
                asm volatile("" : "+r"(f));  // NOLINT
            }
        },
        print("As posting_list_view", count));

    irk::run_with_timer<std::chrono::nanoseconds>(
        [&] {
            auto posting_list = index.postings(term_id).scored(
                index.term_scorer(term_id, irk::score::bm25));
            auto it = posting_list.begin();
            auto last = posting_list.end();
            for (auto p : posting_list) {
                irk::index::document_t d = p.document();
                double s = p.payload();
                asm volatile("" : "+r"(d));  // NOLINT
                asm volatile("" : "+r"(s));  // NOLINT
            }
        },
        print("Scored posting list BM25", count));

    return 0;
}
