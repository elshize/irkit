#include <iostream>
#include <string>
#include <unordered_set>

#include <CLI/CLI.hpp>
#include <boost/filesystem.hpp>

#include <irkit/index.hpp>

namespace fs = boost::filesystem;

struct valid_scoring_function {
    std::unordered_set<std::string> available_scorers;
    std::string operator()(const std::string& scorer)
    {
        if (available_scorers.find(scorer) == available_scorers.end())
        {
            std::ostringstream str;
            str << "Unknown scorer requested. Must select one of:";
            for (const std::string& s : available_scorers) {
                str << " " << s;
            }
            return str.str();
        }
        return std::string();
    }
};

int main(int argc, char** argv)
{
    int bits = 24;
    std::string dir;
    std::string scorer("bm25");
    std::unordered_set<std::string> available_scorers = {"bm24", "ql"};

    CLI::App app{"Compute impact scores of postings in an inverted index."};
    app.add_option("-d,--index-dir", dir, "index directory", true)
        ->check(CLI::ExistingDirectory);
    app.add_option("-b,--bits", bits, "number of bits for a score", true);
    app.add_option("scorer", scorer, "scoring function", true)
        ->check(valid_scoring_function{available_scorers});

    CLI11_PARSE(app, argc, argv);

    fs::path dir_path(dir);
    if (scorer == "bm25") {
        irk::v2::score_index<irk::score::bm25_scorer>(dir, bits);
    } else if (scorer == "ql") {
        irk::v2::score_index<irk::score::query_likelihood_scorer>(dir, bits);
    }

    return 0;
}
