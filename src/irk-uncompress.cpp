#include <experimental/filesystem>
#include <iostream>
#include "cmd.hpp"
#include "irkit/alphabetical_bst.hpp"
#include "irkit/bitstream.hpp"
#include "irkit/coding/hutucker.hpp"

namespace fs = std::experimental::filesystem;


std::vector<std::size_t> frequencies(fs::path file)
{
    std::ifstream f(file);
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
        if (!program.parse(argc, argv)) {
            return 0;
        }
    } catch (irk::po::error& e) {
        std::cout << e.what() << std::endl;
    }

    fs::path input_file(program.get<std::string>("input").value());
    fs::path output_file(program.get<std::string>("output").value());

    std::ifstream fin(input_file);

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
    std::ofstream fout(output_file);
    codec.decode(source, fout, symbols);
    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    std::cerr << "Finished decoding " << symbols << " bytes.\n";
    std::cout << "Elasped time: " << elapsed.count() << " ms" << std::endl;

    fout.close();
    fin.close();
}
