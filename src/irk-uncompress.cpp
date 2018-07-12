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
#include "irkit/alphabetical_bst.hpp"
#include "irkit/bitstream.hpp"
#include "irkit/coding/hutucker.hpp"

namespace fs = boost::filesystem;


std::vector<std::size_t> frequencies(fs::path file)
{
    std::ifstream f(file.c_str());
    auto freqs = irk::coding::huffman::symbol_frequencies(f);
    f.close();
    return freqs;
}

int main(int argc, char** argv)
{
    irk::CmdLineProgram program("irk-compress");
    program.flag("help", "print out help message")
        .arg<std::string>("input", "input file", 1)
        .arg<std::string>("output", "output file", 1);
    try {
        if (not program.parse(argc, argv)) {
            return 0;
        }
    } catch (irk::po::error& e) {
        std::cout << e.what() << std::endl;
    }

    fs::path input_file(program.get<std::string>("input").value());
    fs::path output_file(program.get<std::string>("output").value());

    std::ifstream fin(input_file.c_str());

    auto start_time = std::chrono::steady_clock::now();

    // Read tree along with its size
    std::size_t tree_size;
    fin.read(reinterpret_cast<char*>(&tree_size), sizeof(tree_size));
    std::cerr << "Hu-Tucker tree size: " << tree_size << "\n";
    std::vector<char> tree_bytes(tree_size);
    fin.read(tree_bytes.data(), tree_size);
    irk::alphabetical_bst<char, uint16_t> tree(tree_bytes);
    irk::coding::hutucker_codec<char> codec(tree);

    // Read the number of encoded symbols
    std::size_t symbols;
    fin.read(reinterpret_cast<char*>(&symbols), sizeof(symbols));
    std::cerr << "Uncompressing " << symbols << " encoded bytes.\n";

    // Decode content
    irk::input_bit_stream source(fin);
    std::ofstream fout(output_file.c_str());
    codec.decode(source, fout, symbols);
    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    std::cerr << "Finished decoding " << symbols << " bytes.\n";
    std::cout << "Elasped time: " << elapsed.count() << " ms" << std::endl;

    fout.close();
    fin.close();
}
