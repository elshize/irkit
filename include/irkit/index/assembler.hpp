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

//! \file assembler.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <algorithm>
#include <iostream>
#include <optional>
#include <unordered_map>
#include <vector>
#include <irkit/index/metadata.hpp>
#include <irkit/index/builder.hpp>
#include <irkit/index/merger.hpp>

namespace irk::index {

//! Builds an index in batches and merges them together on disk.
template<class Doc = std::size_t,
    class Term = std::string,
    class TermId = std::size_t,
    class Freq = std::size_t>
class index_assembler {
public:
    using document_type = Doc;
    using term_type = Term;
    using term_id_type = TermId;
    using frequency_type = Freq;
    using builder_type = irk::index_builder<
        document_type, term_type, term_id_type, frequency_type>;
    using merger_type = irk::index_merger<
        document_type, term_type, term_id_type, frequency_type>;

private:
    fs::path output_dir_;
    int batch_size_;

public:
    index_assembler(fs::path output_dir, int batch_size) noexcept
        : output_dir_(output_dir),
          batch_size_(batch_size)
    {}

    void assemble(std::istream& input)
    {
        if (!fs::exists(output_dir_)) { fs::create_directory(output_dir_); }
        fs::path work_dir = output_dir_ / ".batches";
        if (!fs::exists(work_dir)) { fs::create_directory(work_dir); }

        int batch_number = 0;
        std::vector<fs::path> batch_dirs;
        while (input && input.peek() != EOF)
        {
            std::clog << "Building batch " << batch_number << std::endl;
            fs::path batch_dir = work_dir / std::to_string(batch_number);
            metadata batch_metadata(batch_dir);
            build_batch(input, batch_metadata);
            batch_dirs.push_back(std::move(batch_dir));
            ++batch_number;
        }
        std::clog << "Merging " << batch_number << " batches" << std::endl;
        irk::index_merger merger(output_dir_, batch_dirs);
        merger.merge_titles();
        merger.merge_terms();
        std::clog << "Success!" << std::endl;
    }

    std::istream&
    build_batch(std::istream& input, metadata batch_metadata) const
    {
        if (!fs::exists(batch_metadata.dir))
        {
            fs::create_directory(batch_metadata.dir);
        }

        std::ofstream of_doc_ids(batch_metadata.doc_ids.c_str());
        std::ofstream of_doc_ids_off(batch_metadata.doc_ids_off.c_str());
        std::ofstream of_doc_counts(batch_metadata.doc_counts.c_str());
        std::ofstream of_doc_counts_off(batch_metadata.doc_counts_off.c_str());
        std::ofstream of_terms(batch_metadata.terms.c_str());
        std::ofstream of_term_doc_freq(batch_metadata.term_doc_freq.c_str());
        std::ofstream of_titles(batch_metadata.doc_titles.c_str());

        builder_type builder;
        std::string line;
        for (int processed_documents_ = 0;
             processed_documents_ < batch_size_;
             processed_documents_++)
        {
            if (!std::getline(input, line)) { break; }
            document_type doc = document_type(processed_documents_);
            builder.add_document(doc++);
            std::istringstream linestream(line);
            std::string title;
            linestream >> title;
            of_titles << title << '\n';
            std::string term;
            while (linestream >> term) { builder.add_term(term); }
        }

        builder.sort_terms();
        builder.write_terms(of_terms);
        builder.write_document_frequencies(of_term_doc_freq);
        builder.write_document_ids(of_doc_ids, of_doc_ids_off);
        builder.write_document_counts(of_doc_counts, of_doc_counts_off);

        return input;
    }
};

using default_index_assembler =
    index_assembler<std::uint32_t, std::string, std::uint32_t, std::uint32_t>;

};  // namespace irk::index
