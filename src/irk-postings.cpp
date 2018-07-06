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

#include <CLI/CLI.hpp>
#include <boost/filesystem.hpp>

#include <irkit/coding/vbyte.hpp>
#include <irkit/index.hpp>
#include <irkit/index/types.hpp>
#include <irkit/index/source.hpp>

namespace fs = boost::filesystem;

template<class PostingListT>
void print_postings(const PostingListT& postings)
{
    for (const auto& posting : postings)
    { std::cout << posting.document() << "\t" << posting.payload() << "\n"; }
}

int main(int argc, char** argv)
{
    std::string dir = ".";
    std::string term;
    std::string scoring;
    bool use_id = false;

    CLI::App app{"Prints postings."
                 "Fist column: document IDs. Second column: payload."};
    app.add_option("-d,--index-dir", dir, "index directory", true)
        ->check(CLI::ExistingDirectory);
    app.add_flag("-i,--use-id", use_id, "use a term ID");
    app.add_option("-s,--scores",
        scoring,
        "print given scores instead of frequencies",
        true);
    app.add_option("term", term, "term to look up", false)->required();

    CLI11_PARSE(app, argc, argv);

    bool scores_defined = app.count("--scores") > 0;

    try {
        irk::inverted_index_disk_data_source data(fs::path{dir},
            scores_defined ? std::make_optional(scoring) : std::nullopt);
        irk::inverted_index_view index(&data);

        long term_id = use_id ? std::stol(term) : *index.term_id(term);
        if (app.count("--scores") > 0) {
            print_postings(index.scored_postings(term_id));
        } else {
            print_postings(index.postings(term_id));
        }
    } catch (std::bad_optional_access e) {
        std::cerr << "Term " << term << " not found." << std::endl;
    }

}
