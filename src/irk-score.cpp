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
#include <string>
#include <unordered_set>

#include <CLI/CLI.hpp>
#include <boost/filesystem.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <tbb/task_scheduler_init.h>

#include <irkit/index.hpp>
#include <irkit/index/score.hpp>
#include <irkit/index/source.hpp>

namespace fs = boost::filesystem;
using source_type = irk::inverted_index_mapped_data_source;

struct valid_scoring_function {
    std::unordered_set<std::string> available_scorers;
    std::string operator()(const std::string& scorer)
    {
        if (available_scorers.find(scorer) == available_scorers.end())
        {
            std::ostringstream str;
            str << "Unknown scorer requested. Must select one of:";
            for (const std::string& s : available_scorers) {
                str << " " << s;
            }
            return str.str();
        }
        return std::string();
    }
};

int main(int argc, char** argv)
{
    int bits = 24;
    std::string dir;
    std::string scorer("bm25");
    std::unordered_set<std::string> available_scorers = {"bm25", "ql"};
    auto log = spdlog::stderr_color_mt("score");
    int threads = tbb::task_scheduler_init::default_num_threads();

    CLI::App app{"Compute impact scores of postings in an inverted index."};
    app.add_option("-d,--index-dir", dir, "index directory", true)
        ->check(CLI::ExistingDirectory);
    app.add_option("-b,--bits", bits, "number of bits for a score", true);
    app.add_option("-j,--threads", threads, "number of threads", true);
    app.add_option("scorer", scorer, "scoring function", true)
        ->check(valid_scoring_function{available_scorers});
    CLI11_PARSE(app, argc, argv);

    tbb::task_scheduler_init init(threads);
    log->info("Initiating scoring using {} threads", threads);

    fs::path dir_path(dir);
    if (scorer == "bm25") {
        irk::index::score_index<irk::score::bm25_scorer, source_type>(
            dir, bits);
    } else if (scorer == "ql") {
        irk::index::
            score_index<irk::score::query_likelihood_scorer, source_type>(
                dir, bits);
    }

    return 0;
}
