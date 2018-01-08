#include <experimental/filesystem>
#include <iostream>
#include "irkit/index.hpp"
#include "irkit/taat.hpp"

namespace fs = std::experimental::filesystem;

template<class Score>
std::pair<std::vector<std::string>, std::vector<Score>> parse(std::string query)
{
    std::vector<std::string> terms;
    std::vector<Score> weights;
    std::istringstream stream(query);
    std::string term;
    while (stream >> term) {
        terms.push_back(term);
        weights.push_back(Score(1));
    }
    return std::make_pair(terms, weights);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "usage: run_queries <index_dir>\n";
        exit(1);
    }

    fs::path index_dir(argv[1]);

    std::cerr << "Loading index... ";
    irkit::DefaultIndex idx(index_dir);
    std::cerr << "Done.\n";

    std::string line;
    while (std::getline(std::cin, line)) {
        std::cerr << "Running query: " << line << std::endl;
        auto[terms, weights] = parse<double>(line);
        auto postings = idx.posting_ranges(terms);
        for (auto p : postings) {
            std::cerr << p.size() << std::endl;
        }
        //irkit::taat(postings, 10, weights, idx.collection_size());
    }
}

