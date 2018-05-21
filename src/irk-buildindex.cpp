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

//! \file irk-buildindex.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#include <string>
#include <vector>

#include <CLI/CLI.hpp>
#include <boost/filesystem.hpp>

#include <irkit/index/assembler.hpp>

namespace fs = boost::filesystem;

int main(int argc, char** argv)
{
    std::string output_dir;
    int batch_size = 100'000;

    CLI::App app{"Build an inverted index."};
    app.add_option("--batch-size,-b",
        batch_size,
        "Max number of documents to build in memory.",
        true);
    app.add_option("output_dir", output_dir, "Index output directory", false)
        ->required();

    CLI11_PARSE(app, argc, argv);

    irk::index::default_index_assembler assembler(
        fs::path(output_dir), batch_size);
    assembler.assemble(std::cin);
    return 0;
}
