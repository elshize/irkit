#include <boost/filesystem.hpp>
#include <fstream>
#include <gumbo.h>
#include <iomanip>
#include <iostream>
#include <regex>
#include <stdio.h>
#include "cmd.hpp"

namespace fs = boost::filesystem;

std::ofstream& new_file(std::ofstream& out, std::string prefix, std::size_t num, std::size_t padding)
{
    if (out.is_open()) {
        out.close();
    }
    std::ostringstream filename;
    filename << prefix << "-" << std::setfill('0') << std::setw(padding) << num;
    out.open(filename.str());
    return out;
}

int main(int argc, char** argv)
{
    irk::CmdLineProgram program("irk-warc");
    program.flag("help,h", "print help")
        .flag("no-header", "the input file has no header")
        .option<std::size_t>(
            "padding,p", "number of zeroes to use for padding in names", 4)
        .option<std::string>("output,o", "output files prefix")
        .arg<std::size_t>("limit", "number of lines per file", 1)
        .arg<std::vector<std::string>>("input", "input files", -1);
    try {
        if (!program.parse(argc, argv)) {
            return 0;
        }
    } catch (irk::po::error& e) {
        std::cout << e.what() << std::endl;
    }

    std::vector<std::string> input_files;
    if (program.get<std::vector<std::string>>("input") == std::nullopt) {
        input_files.push_back("");
        if (program.get<std::string>("output") == std::nullopt) {
            std::cerr << "you must define -o/--output when reading from stdin"
                      << std::endl;
            return 1;
        }
    } else {
        input_files = program.get<std::vector<std::string>>("input").value();
        if (program.get<std::string>("output") == std::nullopt
            && input_files.size() > 1) {
            std::cerr
                << "you must define -o/--output when reading multiple files"
                << std::endl;
            return 1;
        }
    }

    bool use_header = !program.defined("no-header");
    std::size_t line_num = 0;
    std::size_t file_num = 0;
    std::size_t limit = program.get<std::size_t>("limit").value();
    std::string output_prefix =
        program.get<std::string>("output").value_or(input_files[0]);
    std::size_t padding = program.get<std::size_t>("padding").value();

    std::ofstream out;
    std::optional<std::string> header;

    for (std::string& input_file : input_files) {
        std::istream* in;
        if (input_file != "") {
            in = new std::ifstream(input_file);
        } else {
            in = &std::cin;
        }

        std::string line;

        if (use_header) {
            std::optional<std::string> old_header = header;
            std::getline(*in, line);
            header = std::make_optional(line);
            if (old_header.has_value() && old_header != header) {
                header = old_header;
            }
        }

        while (std::getline(*in, line)) {
            if (line_num == 0) {
                new_file(out, output_prefix, file_num++, padding);
                if (header.has_value()) {
                    out << header.value() << std::endl;
                }
            }
            out << line << std::endl;
            line_num = (line_num + 1) % limit;
        }
        if (input_file != "") {
            delete in;
        }
    }
    out.close();
}

