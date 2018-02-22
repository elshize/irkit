#include <boost/filesystem.hpp>
#include <iostream>
#include "irkit/index.hpp"

namespace fs = boost::filesystem;

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cerr << "usage: irk-score <index_dir> <scoring>\n";
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


    return 0;
}
