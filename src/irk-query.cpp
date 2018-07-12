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

#include <boost/filesystem.hpp>
#include <chrono>
#include <iostream>
#include "cmd.hpp"
#include "irkit/index.hpp"
#include "irkit/taat.hpp"

namespace fs = boost::filesystem;

template<class Score>
std::pair<std::vector<std::string>, std::vector<Score>> parse(std::string query)
{
    //struct SN_env * z;
    //z = Khotanese_create_env();
    std::vector<std::string> terms;
    std::vector<Score> weights;
    std::istringstream stream(query);
    std::string term;
    while (stream >> term) {
        terms.push_back(term);
        weights.push_back(Score(1));
    }
    return std::make_pair(terms, weights);
}

int main(int argc, char** argv)
{
    irk::CmdLineProgram program("irk-query");
    program.flag("stem,s", "perform stemming on the input query terms")
        .option<std::string>("index-dir,d", "index base directory", ".");
    try {
        if (not program.parse(argc, argv)) {
            return 0;
        }
    } catch (irk::po::error& e) {
        std::cout << e.what() << std::endl;
    }

    fs::path dir(program.get<std::string>("index-dir").value());
    //bool stem = program.defined("stem");

    std::cerr << "Loading index... ";
    std::unique_ptr<irk::default_index> index(nullptr);
    try {
        index.reset(new irk::default_index(dir, false));
    } catch (std::invalid_argument& e) {
        std::cerr << e.what() << " (not an index directory?)" << std::endl;
        std::exit(1);
    }
    std::cerr << "Done.\n";

    std::string line;
    std::cout << "> ";
    while (std::getline(std::cin, line)) {
        std::cout << "Running query: " << line << std::endl;
        auto start_time = std::chrono::steady_clock::now();
        auto[terms, weights] = parse<double>(line);
        auto postings = index->posting_ranges(terms);
        auto results =
            irk::taat(postings, 10, weights, index->collection_size());
        auto end_time = std::chrono::steady_clock::now();
        for (auto& result : results) {
            std::cout << result << " (" << index->title(result.doc) << ")"
                      << std::endl;
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        std::cout << "Elasped time: " << elapsed.count() << " ms" << std::endl;
        std::cout << "> ";
    }
}
