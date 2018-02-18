#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <chrono>
#include <experimental/filesystem>
#include <fstream>
#include <gumbo.h>
#include <iostream>
#include <regex>
#include <stdio.h>
#include "cmd.hpp"
#include "irkit/io/warc.hpp"
#include "irkit/parsing/html.hpp"
#include "irkit/parsing/snowball/porter2.hpp"

namespace fs = std::experimental::filesystem;

void write_term(std::string&& term, SN_env* z, bool lowercase)
{
    if (z != nullptr) {
        SN_set_current(
            z, term.size(), reinterpret_cast<unsigned char*>(term.data()));
        stem(z);
        if (lowercase) {
            for (int idx = 0; idx < z->l; ++idx) {
                z->p[idx] = tolower(z->p[idx]);
            }
        }
        z->p[z->l] = 0;
        std::cout << z->p;
    } else {
        std::cout << term;
    }
}

int main(int argc, char** argv)
{
    irk::CmdLineProgram program("irk-warc");
    program.flag("zip,z", "use zipped input files")
        .flag("stem,s", "stem terms")
        .flag("lowercase,l", "transform all characters to lower case")
        .flag("help,h", "print help")
        .flag("skip-header", "skip header defining column names")
        .option<std::string>("fieldsep,f", "header field separator", "\t")
        .arg<std::vector<std::string>>("input", "input WARC files", -1);
    try {
        if (!program.parse(argc, argv)) {
            return 0;
        }
    } catch (irk::po::error& e) {
        std::cout << e.what() << std::endl;
    }

    std::string field_separator = program.get<std::string>("fieldsep").value();
    bool lowercase = program.defined("lowercase");

    if (program.get<std::vector<std::string>>("input") == std::nullopt) {
        std::cerr << "you must define at least one input file\n";
        return 1;
    } else {
        SN_env * z = nullptr;
        if (program.defined("stem")) {
            z = create_env();
        }

        if (!program.defined("skip-header")) {
            std::cout << "title\turl\tsize\tbody\n";
        }

        std::vector<std::string> input_files =
            program.get<std::vector<std::string>>("input").value();

        for (auto& input_file : input_files) {
            std::ifstream fin(input_file);
            boost::iostreams::filtering_istream in;
            if (program.defined("zip")) {
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
                        for (; term_iter != std::sregex_iterator();
                             term_iter++) {
                            std::cout << " ";
                            write_term(term_iter->str(), z, lowercase);
                        }
                        std::cout << std::endl;
                    }
                }
            }
        }
        if (program.defined("stem")) {
            close_env(z);
        }
    }

}

