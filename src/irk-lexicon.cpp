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

#include <CLI/CLI.hpp>
#include <boost/filesystem.hpp>

#include <irkit/lexicon.hpp>
#include <irkit/memoryview.hpp>

void run_build(const std::string& input,
    const std::string& output,
    std::ptrdiff_t keys_per_block)
{
    auto lexicon = irk::build_lexicon(input, keys_per_block);
    std::ofstream out(output);
    lexicon.serialize(out);
}

void run_lookup(const std::string& lexicon_file, const std::string& key)
{
    auto lex = irk::load_lexicon(
        irk::make_memory_view(boost::filesystem::path(lexicon_file)));
    auto idx = lex.index_at(key);
    if (idx.has_value()) {
        std::cout << idx.value() << std::endl;
    } else {
        std::cout << "Not found" << std::endl;
    }
}

int main(int argc, char** argv)
{
    std::string input, lexicon_file, string_key;
    std::ptrdiff_t keys_per_block = 128;

    CLI::App app{"Builds a lexicon (string to positional index mapping)."};
    app.require_subcommand(1);

    CLI::App* build = app.add_subcommand("build", "Build a lexicon");
    build->add_option(
        "-b,--keys-per-block", keys_per_block, "keys per block", true);
    build->add_option("input", input, "input file", false)
        ->check(CLI::ExistingFile)
        ->required();
    build->add_option("output", lexicon_file, "output", false)->required();

    CLI::App* lookup = app.add_subcommand(
        "lookup", "Resolve the index of a string.");
    lookup->add_option("lexicon", lexicon_file, "Lexicon file", false)
        ->required()
        ->check(CLI::ExistingFile);
    lookup->add_option("string-key", string_key, "A string key to resolve")
        ->required();

    CLI11_PARSE(app, argc, argv);

    if (*lookup) {
        run_lookup(lexicon_file, string_key);
        return 0;
    }

    if (*build) {
        run_build(input, lexicon_file, keys_per_block);
        return 0;
    }
}
