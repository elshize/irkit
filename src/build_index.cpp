#include <experimental/filesystem>
#include <iostream>
#include "irkit/index.hpp"

namespace fs = std::experimental::filesystem;

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "usage: build_index <output_dir>\n";
        exit(1);
    }

    fs::path output_dir(argv[1]);

    fs::path doc_ids = irkit::index::doc_ids_path(output_dir);
    fs::path doc_ids_off = irkit::index::doc_ids_off_path(output_dir);
    fs::path doc_counts = irkit::index::doc_counts_path(output_dir);
    fs::path doc_counts_off = irkit::index::doc_counts_off_path(output_dir);
    fs::path terms = irkit::index::terms_path(output_dir);
    fs::path term_doc_freq = irkit::index::term_doc_freq_path(output_dir);
    fs::path doc_titles = irkit::index::titles_path(output_dir);

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

    irkit::DefaultIndexBuilder builder;
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
    }
    of_titles.close();

    builder.sort_terms();
    builder.write_terms(of_terms);
    builder.write_document_frequencies(of_term_doc_freq);
    builder.write_document_ids(of_doc_ids, of_doc_ids_off);
    builder.write_document_counts(of_doc_counts, of_doc_counts_off);

    of_doc_ids.close();
    of_doc_ids_off.close();
    of_doc_counts.close();
    of_doc_counts_off.close();
    of_terms.close();
    of_term_doc_freq.close();
}

