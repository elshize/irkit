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
#include <irkit/index/source.hpp>
#include <irkit/index/types.hpp>

namespace fs = boost::filesystem;
using irk::index::term_id_t;

template<class PostingListT>
void print_postings(const PostingListT& postings,
    bool use_titles,
    const irk::inverted_index_view& index)
{
    for (const auto& posting : postings)
    {
        std::cout << posting.document() << "\t";
        if (use_titles) {
            std::cout << index.titles().key_at(posting.document()) << "\t";
        }
        std::cout << posting.payload() << "\n";
    }
}

int main(int argc, char** argv)
{
    std::string dir = ".";
    std::string term;
    std::string scoring;
    bool use_id = false;
    bool use_titles = false;

    CLI::App app{"Prints postings."
                 "Fist column: document IDs. Second column: payload."};
    app.add_option("-d,--index-dir", dir, "index directory", true)
        ->check(CLI::ExistingDirectory);
    app.add_flag("-i,--use-id", use_id, "use a term ID");
    app.add_flag("-t,--titles", use_titles, "print document titles");
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

        term_id_t term_id = use_id ? std::stoi(term)
                                   : index.term_id(term).value();
        if (app.count("--scores") > 0) {
            print_postings(index.scored_postings(term_id), use_titles, index);
        } else {
            print_postings(index.postings(term_id), use_titles, index);
        }
    } catch (const std::bad_optional_access& e) {
        std::cerr << "Term " << term << " not found." << std::endl;
    }

}
