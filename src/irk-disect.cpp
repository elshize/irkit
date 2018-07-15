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

#include <irkit/coding/stream_vbyte.hpp>
#include <irkit/coding/vbyte.hpp>
#include <irkit/index.hpp>
#include <irkit/index/source.hpp>
#include <irkit/index/types.hpp>
#include <irkit/parsing/stemmer.hpp>

namespace fs = boost::filesystem;
using irk::index::term_id_t;
using irk::index::document_t;

void disect_document_list(const irk::memory_view memory, int64_t length)
{
    auto pos = memory.begin();
    irk::vbyte_codec<int64_t> vb;
    irk::stream_vbyte_codec<document_t> codec;

    int64_t list_byte_size, num_blocks, block_size;
    pos = vb.decode(pos, &list_byte_size);
    pos = vb.decode(pos, &block_size);
    pos = vb.decode(pos, &num_blocks);
    if (list_byte_size != memory.size())
    {
        std::ostringstream str;
        str << "list size " << list_byte_size
            << " does not match memory view size " << memory.size();
        throw std::runtime_error(str.str());
    }

    std::cout << "List size in bytes: " << list_byte_size << std::endl;
    std::cout << "Block size: " << block_size << std::endl;
    std::cout << "Block count: " << num_blocks << std::endl;

    std::vector<int64_t> skips(num_blocks);
    pos = vb.decode(pos, &skips[0], num_blocks);
    std::vector<document_t> last_documents(num_blocks);
    pos = codec.delta_decode(pos, &last_documents[0], num_blocks);

    std::cout << "Skips: [ ";
    for (const auto& skip : skips) { std::cout << skip << " "; }
    std::cout << "]\nLast doc in block: [ ";
    for (const auto& doc : last_documents) { std::cout << doc << " "; }
    std::cout << std::endl;

    for (int block = 0; block < num_blocks - 1; block++)
    {
        std::advance(pos, skips[block]);
        auto mem = irk::make_memory_view(pos, skips[block + 1]);
        auto count = block < num_blocks - 1 ? block_size : length % block_size;
        auto preceding = block > 0 ? last_documents[block - 1] : 0;
        std::vector<document_t> decoded(count);
        codec.delta_decode(
            std::begin(mem), std::begin(decoded), count, preceding);
        std::cout << "B" << block << ": [ ";
        for (const auto& doc : decoded) { std::cout << doc << " "; }
        std::cout << "]\n";
    }
    std::advance(pos, skips.back());
    auto mem = irk::make_memory_view(pos, std::distance(pos, std::end(memory)));
    auto count = length % block_size;
    std::vector<document_t> decoded(count);
    codec.delta_decode(std::begin(mem),
        std::begin(decoded),
        count,
        last_documents[num_blocks - 1]);
    std::cout << "B" << num_blocks - 1 << ": [ ";
    for (const auto& doc : decoded) { std::cout << doc << " "; }
    std::cout << "]\n";
}

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
    bool stem = false;

    CLI::App app{"Disects a posting list."};
    app.add_option("-d,--index-dir", dir, "index directory", true)
        ->check(CLI::ExistingDirectory);
    app.add_flag("-i,--use-id", use_id, "use a term ID");
    app.add_flag("-t,--titles", use_titles, "print document titles");
    app.add_flag("--stem", stem, "Stem terems (Porter2)");
    app.add_option(
        "--scores", scoring, "print given scores instead of frequencies", true);
    app.add_option("term", term, "term to look up", false)->required();
    CLI11_PARSE(app, argc, argv);

    if (not use_id && stem)
    {
        irk::porter2_stemmer stemmer;
        term = stemmer.stem(term);
    }

    bool scores_defined = app.count("--scores") > 0;

    try {
        irk::inverted_index_mapped_data_source data(fs::path{dir},
            scores_defined ? std::make_optional(scoring) : std::nullopt);
        irk::inverted_index_view index(&data);

        term_id_t term_id = use_id ? std::stoi(term)
                                   : index.term_id(term).value();
        disect_document_list(
            index.documents(term_id).memory(), index.tdf(term_id));
    } catch (const std::bad_optional_access& e) {
        std::cerr << "Term " << term << " not found." << std::endl;
    }

}
