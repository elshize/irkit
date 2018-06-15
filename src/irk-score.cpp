#include <iostream>
#include <string>

#include <CLI/CLI.hpp>
#include <boost/filesystem.hpp>

#include <irkit/index.hpp>

namespace fs = boost::filesystem;

int main(int argc, char** argv)
{
    int bits = 24;
    std::string dir;
    CLI::App app{"Compute impact scores of postings in an inverted index."};
    app.add_option("-d,--index-dir", dir, "index directory", true)
        ->check(CLI::ExistingDirectory);
    app.add_option("-b,--bits", bits, "number of bits for a score", true);

    CLI11_PARSE(app, argc, argv);

    fs::path dir_path(dir);
    irk::v2::score_index(dir, bits);

    return 0;
}
