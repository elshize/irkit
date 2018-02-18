#include <experimental/filesystem>
#include <iostream>
#include "cmd.hpp"
#include "irkit/index.hpp"

namespace fs = std::experimental::filesystem;

int main(int argc, char** argv)
{
    irk::CmdLineProgram program("irk-postings");
    program.flag("use-term-ids", "use term IDs as input")
        .flag("help", "print out help message")
        .option<std::string>("index-dir,d", "index base directory", ".")
        .arg<std::string>("term", "the term to look up", -1);
    try {
        if (!program.parse(argc, argv)) {
            return 0;
        }
    } catch (irk::po::error& e) {
        std::cout << e.what() << std::endl;
    }

    fs::path dir(program.get<std::string>("index-dir").value());

    std::unique_ptr<irk::default_index> index(nullptr);
    try {
        index.reset(new irk::default_index(dir, false));
    } catch (std::invalid_argument& e) {
        std::cerr << e.what() << " (not an index directory?)" << std::endl;
        std::exit(1);
    }

    if (program.defined("use-term-ids")) {
        //auto postings = index.posting_range(
    } else {
        auto postings =
            index->posting_range(program.get<std::string>("term").value());
        for (auto posting : postings) {
            std::cout << posting.doc << "\t" << posting.score << std::endl;
        }
    }

}
