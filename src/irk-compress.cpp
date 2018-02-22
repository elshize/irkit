#include <boost/filesystem.hpp>
#include <chrono>
#include <iostream>
#include "cmd.hpp"
#include "irkit/bitstream.hpp"
#include "irkit/coding/hutucker.hpp"

namespace fs = boost::filesystem;

std::vector<std::size_t>
frequencies(fs::path file)
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
        if (!program.parse(argc, argv)) {
            return 0;
        }
    } catch (irk::po::error& e) {
        std::cout << e.what() << std::endl;
    }

    fs::path input_file(program.get<std::string>("input").value());
    fs::path output_file(program.get<std::string>("output").value());

    auto start_time = std::chrono::steady_clock::now();

    auto size = fs::file_size(input_file);
    irk::coding::hutucker_codec<char> codec(frequencies(input_file));
    std::ifstream fin(input_file.c_str());
    std::ofstream fout(output_file.c_str());

    // Encode tree (preceeding by its size)
    auto tree_bytes = codec.tree().memory_container();
    auto tree_size = tree_bytes.size();
    std::cerr << "Writing tree size: " << tree_size << "\n";
    fout.write(reinterpret_cast<char*>(&tree_size), sizeof(tree_size));
    fout.write(tree_bytes.data(), tree_size);

    // Encode the number of symbols
    std::cerr << "Writing file size: " << size << "\n";
    fout.write(reinterpret_cast<char*>(&size), sizeof(size));

    // Encode content
    irk::output_bit_stream sink(fout);
    auto encoded_symbols = codec.encode(fin, sink);
    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    std::cerr << "Finished encoding " << encoded_symbols << " bytes.\n";
    std::cout << "Elasped time: " << elapsed.count() << " ms" << std::endl;

    sink.flush();
    fout.close();
    fin.close();
}
