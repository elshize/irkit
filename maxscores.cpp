#include <algorithm>
#include <cmath>
#include <iostream>
#include "type_safe/strong_typedef.hpp"
#include "index.hpp"

using namespace bloodhound;

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "usage: maxscores <index_dir> [#bins=10]\nUSE ON A SORTED "
                     "INDEX!";
        exit(1);
    }
    std::size_t nbins = 10;
    if (argc > 3) {
        nbins = std::stoi(argv[2]);
    }

    fs::path index_dir(argv[1]);
    index::Index index = index::Index<>::load_index(index_dir);

    std::cout << "term,bin,count\n";
    std::size_t term_idx = 0;
    for (auto it = index.lexicon.begin(); it != index.lexicon.end(); ++it) {
        auto term = it->first;
        auto postlist = index.posting_list(term);
        auto len = postlist.length();
        if (len == 0) continue;
        auto scoreit = postlist.score_begin();
        uint32_t max = type_safe::get(*scoreit);
        std::vector<std::size_t> bins(nbins, 0);
        for (; scoreit != postlist.score_end(); scoreit++) {
            uint32_t score = type_safe::get(*scoreit);
            std::size_t bin = score * nbins / (max + 1);
            bins[bin]++;
        }
        for (std::size_t bin = 0; bin < nbins; ++bin) {
            std::cout << term_idx << "," << bin << "," << bins[bin]
                      << std::endl;
        }
    }
    return 0;
}
