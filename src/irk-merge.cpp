#include <boost/filesystem.hpp>
#include <iostream>
#include "irkit/index.hpp"
#include "irkit/index/merger.hpp"

namespace fs = boost::filesystem;

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cerr << "usage: irk-merge <target_index_dir> [<parts>...]\n";
        exit(1);
    }

    fs::path target_dir(argv[1]);
    if (!fs::exists(target_dir)) {
        fs::create_directory(target_dir);
    }
    std::vector<fs::path> parts;
    for (int arg = 2; arg < argc; ++arg) {
        parts.emplace_back(argv[arg]);
    }

    irk::default_index_merger merger(target_dir, parts, true);
    std::cout << "Merging titles... ";
    std::cout.flush();
    merger.merge_titles();
    std::cout << "done.\nMerging terms... ";
    std::cout.flush();
    merger.merge_terms();
    std::cout << "done.\n";

    return 0;
}