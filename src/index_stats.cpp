#include <algorithm>
#include <cmath>
#include <iostream>
#include "type_safe/strong_typedef.hpp"
#include "index.hpp"

using namespace bloodhound;

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "usage: index_stats <index_dir>\n";
        exit(1);
    }

    fs::path index_dir(argv[1]);
    index::Index index = index::Index<>::load_index(index_dir);

    std::cout << "min_score,score_25,median_score,score_75,max_score,avg_score,"
                 "sd_score\n";
    for (auto it = index.lexicon.begin(); it != index.lexicon.end(); ++it) {
        auto postlist = index.posting_list(it->first);
        auto len = postlist.length();
        if (len == 0) continue;
        uint32_t max_score = type_safe::get(*postlist.score_begin());
        uint32_t score_25 =
            type_safe::get(*(postlist.score_begin() + (3 * len / 4)));
        uint32_t median_score =
            type_safe::get(*(postlist.score_begin() + (len / 2)));
        uint32_t score_75 =
            type_safe::get(*(postlist.score_begin() + (len / 4)));
        uint32_t min_score =
            type_safe::get(*(postlist.score_begin() + len - 1));
        double avg_score = 0;
        double avg_score_sq = 0;
        for (auto scoreit = postlist.score_begin();
             scoreit != postlist.score_end();
             scoreit++) {
            uint32_t score = type_safe::get(*scoreit);
            avg_score += score;
            avg_score_sq += std::pow(score, 2);
        }
        avg_score /= postlist.length();
        avg_score_sq /= postlist.length();
        double sd_score = std::sqrt(avg_score_sq - std::pow(avg_score, 2));
        std::cout << min_score
            << "," << score_25
            << "," << median_score
            << "," << score_75
            << "," << max_score
            << "," << avg_score
            << "," << sd_score
            << std::endl;
    }
    return 0;
}
