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
#include <sstream>
#include <string>

#include <CLI/CLI.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/console.hpp>

//using boost::log::keywords::format;

int run(const std::string& input_file, std::size_t document_count)
{
    boost::log::add_console_log(std::cerr);
    BOOST_LOG_TRIVIAL(info) << "Allocating memory...";
    std::vector<int> hits(document_count, 0);
    std::ifstream in(input_file);
    std::string line;
    int line_num = 0;
    BOOST_LOG_TRIVIAL(info) << "Starting aggregation...";
    try {
        while (std::getline(in, line))
        {
            std::istringstream linestream(line);
            std::string doc;
            std::cerr << "LINE\n";
            while (linestream >> doc)
            {
                int docid = std::stoi(doc);
                ++hits[docid];
            }
            ++line_num;
        }
    } catch (...) {
        BOOST_LOG_TRIVIAL(error)
            << "Error processing line " << line_num << ": " << line;
    }
    BOOST_LOG_TRIVIAL(info) << "Printing results...";
    for (int h : hits) { std::cout << h << "\n"; }
    return 0;
}

int main(int argc, char** argv)
{
    std::string input;
    std::size_t document_count;

    CLI::App app{"Aggregates document hits.\n"
                 "Input format: each line consists of document IDs "
                 "sorted by their rank for the query, "
                 "separated by any whitespaces.\n"
                 "The hits are printed to standard output, "
                 "while debug logs are printed to standard error outupt."};
    app.add_option("input", input, "Input file", false)->required();
    app.add_option("-c,--document-count",
           document_count,
           "Document count in index",
           false)
        ->required();

    CLI11_PARSE(app, argc, argv);
    return run(input, document_count);
}
