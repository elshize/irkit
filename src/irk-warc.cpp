#include <CLI/CLI.hpp>
#include <boost/filesystem.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <chrono>
#include <fstream>
#include <gumbo.h>
#include <iostream>
#include <regex>
#include <stdio.h>
#include "irkit/io/warc.hpp"
#include "irkit/parsing/html.hpp"
#include "irkit/parsing/snowball/porter2.hpp"

namespace fs = boost::filesystem;

void write_term(std::string&& term, SN_env* z, bool lowercase)
{
    if (lowercase) {
        for (int idx = 0; idx < z->l; ++idx) {
            z->p[idx] = tolower(z->p[idx]);
        }
    }
    if (z != nullptr) {
        SN_set_current(
            z, term.size(), reinterpret_cast<unsigned char*>(&term[0]));
        stem(z);
        z->p[z->l] = 0;
        std::cout << z->p;
    } else {
        std::cout << term;
    }
}

int main(int argc, char** argv)
{
    std::string field_separator = "\t";
    std::vector<std::string> input_files;

    CLI::App app{"Read and parse WARC collections."};
    app.add_flag("-z,--zip", "use zipped input files");
    app.add_flag("-s,--stem", "stem terms");
    app.add_flag("-l,--lowercase", "transform all characters to lower case");
    app.add_flag("--skip-header", "skip header defining column names");
    app.add_option("-f,--fieldsep",
        field_separator,
        "field separator in the output file",
        true);
    app.add_option("input", input_files, "input WARC files", false)
        ->required()
        ->check(CLI::ExistingFile);

    CLI11_PARSE(app, argc, argv);

    bool lowercase = app.count("--lowercase");

    SN_env* z = nullptr;
    if (app.count("--stem")) {
        z = create_env();
    }

    if (!app.count("--skip-header")) {
        std::cout << "title\turl\tsize\tbody\n";
    }

    for (auto& input_file : input_files) {
        std::ifstream fin(input_file);
        boost::iostreams::filtering_istream in;
        if (app.count("--zip")) {
            in.push(boost::iostreams::gzip_decompressor());
        }
        in.push(fin);

        irkit::io::warc_record record;
        while (irkit::io::read_warc_record(in, record)) {
            if (record.type() == "response") {
                std::cout << record.trecid() << field_separator;
                std::cout << record.url() << field_separator;
                std::cout << record.content_length() << field_separator;
                std::string content =
                    irk::parsing::html::cleantext(record.content());
                std::regex term_pattern("(\\S+)");
                auto term_iter = std::sregex_iterator(
                    content.begin(), content.end(), term_pattern);
                if (term_iter != std::sregex_iterator()) {
                    write_term(term_iter->str(), z, lowercase);
                    term_iter++;
                    for (; term_iter != std::sregex_iterator(); term_iter++) {
                        std::cout << " ";
                        write_term(term_iter->str(), z, lowercase);
                    }
                    std::cout << std::endl;
                }
            }
        }
    }
    if (app.count("--stem")) {
        close_env(z);
    }
}

