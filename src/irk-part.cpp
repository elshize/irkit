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

//! \file irk-part.cpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#include <CLI/CLI.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <gumbo.h>
#include <iomanip>
#include <iostream>
#include <regex>
#include <stdio.h>

namespace fs = boost::filesystem;

std::ofstream& new_file(std::ofstream& out,
    std::string prefix,
    std::size_t num,
    std::size_t padding)
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
    struct {
        std::size_t padding_width = 4;
        std::string output;
        std::vector<std::string> input_files;
        std::size_t limit;
    } args;

    CLI::App app{"irk-part: partition a text file by lines"};
    app.add_flag("--no-header", "the input file has no header");
    app.add_option("-p,--padding-width",
        args.padding_width,
        "number of zeroes to use for padding in names",
        true);
    app.add_option(
        "-o,--output", args.output, "the prefix of the output files");
    app.add_option("limit", args.limit, "the number of lines per file", false)
        ->required();
    app.add_option("input", args.input_files, "input files", false)
        ->check(CLI::ExistingFile);

    CLI11_PARSE(app, argc, argv);

    if (!app.count("input")) {
        args.input_files.push_back("");
        if (!app.count("--output")) {
            std::cerr << "you must define --output when reading from stdin"
                      << std::endl;
            return 1;
        }
    } else {
        if (!app.count("output") && args.input_files.size() > 1) {
            std::cerr << "you must define --output when reading multiple files"
                      << std::endl;
            return 1;
        }
    }

    bool use_header = app.count("--no-header");
    std::size_t line_num = 0;
    std::size_t file_num = 0;
    std::string output_prefix =
        app.count("--output") ? args.output : args.input_files[0];

    std::ofstream out;
    std::optional<std::string> header;

    for (std::string& input_file : args.input_files) {
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
                new_file(out, output_prefix, file_num++, args.padding_width);
                if (header.has_value()) {
                    out << header.value() << std::endl;
                }
            }
            out << line << std::endl;
            line_num = (line_num + 1) % args.limit;
        }
        if (input_file != "") {
            delete in;
        }
    }
    out.close();
}

