#include <algorithm>
#include <cmath>
#include <iostream>
#include "type_safe/strong_typedef.hpp"
#include "index.hpp"

using namespace bloodhound;

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "usage: maxscores <index_dir> [<query_file>]"
                  << "\nUSE ON A SORTED INDEX!";
        exit(1);
    }
    fs::path query_path;
    if (argc > 2) {
        query_path = fs::path(argv[2]);
    }

    fs::path index_dir(argv[1]);
    index::Index index = index::Index<>::load_index(index_dir);

    std::cout << "termid,length,maxscore\n";
    if (argc > 2) {
        std::string line;
        std::ifstream query_stream(query_path.c_str());
        std::vector<TermId> terms;
        while (std::getline(query_stream, line)) {
            std::string term_pair;
            std::istringstream line_stream(line);
            while (line_stream >> term_pair) {
                std::size_t pos = term_pair.find(":");
                term_pair[pos] = ' ';
                std::istringstream pair_stream(term_pair);
                TermId termid;
                pair_stream >> termid;
                terms.push_back(termid);
            }
        }
        std::sort(terms.begin(), terms.end());
        auto last = std::unique(terms.begin(), terms.end());
        terms.erase(last, terms.end());
        for (auto& term : terms) {
            auto postlist = index.posting_list(term);
            auto len = postlist.length();
            if (len == 0)
                continue;
            auto scoreit = postlist.score_begin();
            uint32_t max = type_safe::get(*scoreit);
            std::cout << term << "," << len << "," << max << std::endl;
        }
    } else {
        std::size_t term_idx = 0;
        for (auto it = index.lexicon.begin(); it != index.lexicon.end(); ++it) {
            auto term = it->first;
            auto postlist = index.posting_list(term);
            auto len = postlist.length();
            if (len == 0)
                continue;
            auto scoreit = postlist.score_begin();
            uint32_t max = type_safe::get(*scoreit);
            std::cout << term_idx << "," << len << "," << max << std::endl;
        }
    }
    return 0;
}
