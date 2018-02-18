#include <experimental/filesystem>
#include <iostream>
#include "irkit/index.hpp"
#include "irkit/index/builder.hpp"

namespace fs = std::experimental::filesystem;

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "usage: build_index <output_dir>\n";
        exit(1);
    }

    fs::path output_dir(argv[1]);

    fs::path doc_ids = irk::index::doc_ids_path(output_dir);
    fs::path doc_ids_off = irk::index::doc_ids_off_path(output_dir);
    fs::path doc_counts = irk::index::doc_counts_path(output_dir);
    fs::path doc_counts_off = irk::index::doc_counts_off_path(output_dir);
    fs::path terms = irk::index::terms_path(output_dir);
    fs::path term_doc_freq = irk::index::term_doc_freq_path(output_dir);
    fs::path doc_titles = irk::index::titles_path(output_dir);

    if (!fs::exists(output_dir)) {
        fs::create_directory(output_dir);
    }
    std::ofstream of_doc_ids(doc_ids);
    std::ofstream of_doc_ids_off(doc_ids_off);
    std::ofstream of_doc_counts(doc_counts);
    std::ofstream of_doc_counts_off(doc_counts_off);
    std::ofstream of_terms(terms);
    std::ofstream of_term_doc_freq(term_doc_freq);
    std::ofstream of_titles(doc_titles);

    irk::default_index_builder builder;
    std::string line;
    std::size_t doc = 0;
    while (std::getline(std::cin, line)) {
        builder.add_document(doc++);
        std::istringstream linestream(line);
        std::string title;
        linestream >> title;
        of_titles << title << std::endl;
        std::string term;
        while (linestream >> term) {
            builder.add_term(term);
        }
        if (doc % 10000 == 0) {
            std::cerr << "documents: " << doc
                      << "; terms: " << builder.term_count() << std::endl;
        }
    }
    of_titles.close();

    std::cerr << "sorting terms... ";
    builder.sort_terms();
    std::cerr << "done\nwriting terms... ";
    builder.write_terms(of_terms);
    std::cerr << "done\nwriting document frequencies... ";
    builder.write_document_frequencies(of_term_doc_freq);
    std::cerr << "done\nwriting document IDs... ";
    builder.write_document_ids(of_doc_ids, of_doc_ids_off);
    std::cerr << "done\nwriting document counts... ";
    builder.write_document_counts(of_doc_counts, of_doc_counts_off);
    std::cerr << "done\n";

    of_doc_ids.close();
    of_doc_ids_off.close();
    of_doc_counts.close();
    of_doc_counts_off.close();
    of_terms.close();
    of_term_doc_freq.close();
}

