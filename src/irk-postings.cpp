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
#include <numeric>

#include <CLI/CLI.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <range/v3/action/split.hpp>

#include <irkit/coding/vbyte.hpp>
#include <irkit/index.hpp>
#include <irkit/index/source.hpp>
#include <irkit/index/types.hpp>
#include <irkit/parsing/stemmer.hpp>
#include "cli.hpp"

using boost::filesystem::path;
using irk::inverted_index_mapped_data_source;
using irk::inverted_index_view;
using irk::cli::optional;
using irk::index::term_id_t;

template<class PostingListT>
void print_postings(
    const PostingListT& postings,
    bool use_titles,
    const irk::inverted_index_view& index)
{
    for (const auto& posting : postings) {
        std::cout << posting.document() << "\t";
        if (use_titles) {
            std::cout << index.titles().key_at(posting.document()) << "\t";
        }
        std::cout << posting.payload() << "\n";
    }
}

template<class Range>
int64_t
count_postings(const Range& terms, const irk::inverted_index_view& index)
{
    return std::accumulate(
        std::begin(terms),
        std::end(terms),
        int64_t(0),
        [&index](int64_t acc, const std::string& term) {
            return acc + index.postings(term).size();
        });
}

template<class Range, class Args>
void process_query(
    Range& terms,
    const irk::inverted_index_view& index,
    const Args& args,
    bool count)
{
    if (not args.nostem) {
        irk::porter2_stemmer stemmer;
        for (std::string& term : terms) {
            term = stemmer.stem(term);
        }
    }
    if (count) {
        std::cout << count_postings(terms, index) << '\n';
    } else {
        assert(terms.size() == 1u);
        if (args.score_function_defined()) {
            print_postings(index.scored_postings(terms[0]), false, index);
        } else {
            print_postings(index.postings(terms[0]), false, index);
        }
    }
}

int main(int argc, char** argv)
{
    auto [app, args] = irk::cli::app(
        "Print information about term and its posting list",
        irk::cli::index_dir_opt{},
        irk::cli::nostem_opt{},
        // irk::cli::noheader_opt{},
        // irk::cli::sep_opt{},
        irk::cli::score_function_opt{},
        irk::cli::id_range_opt{},
        irk::cli::terms_pos{optional});
    app->add_flag("-c,--count", "Count postings");
    CLI11_PARSE(*app, argc, argv);

    bool count = app->count("--count") == 1u;
    if (not count && args->terms.size() != 1u) {
        std::cerr << "Multiple terms are only supported with --count.\n";
        return 1;
    }

    irk::inverted_index_mapped_data_source data(
        path(args->index_dir),
        args->score_function_defined()
            ? std::make_optional(args->score_function)
            : std::nullopt);
    irk::inverted_index_view index(&data);

    if (not args->terms.empty()) {
        process_query(args->terms, index, *args, count);
        return 0;
    }

    std::string line;
    while (std::getline(std::cin, line)) {
        std::vector<std::string> terms;
        boost::split(
            terms, line, boost::is_any_of("\t "), boost::token_compress_on);
        process_query(terms, index, *args, true);
    }
    return 0;
}
