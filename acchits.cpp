#include <algorithm>
#include <cmath>
#include <iostream>
#include "type_safe/strong_typedef.hpp"
#include "index.hpp"
#include "util.hpp"

using namespace bloodhound;

std::size_t calc_nblocks(std::size_t collection_size, std::size_t block_size) {
    return (collection_size + block_size - 1) / block_size;
}

std::size_t calc_block(Doc doc, std::size_t block_size) {
    return type_safe::get(doc) / block_size;
}

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::cerr << "usage: acchits <index_dir> <query_file>\n";
        exit(1);
    }

    fs::path query_file(argv[2]);
    fs::path index_dir(argv[1]);
    index::Index index = index::Index<>::load_index(index_dir);

    std::vector<std::size_t> block_sizes = {128, 256, 512, 1024};
    std::vector<std::vector<bool>> block_arrays;

    std::cout << "query,acchits";
    for (auto block_size : block_sizes) {
        std::cout << ",b" << block_size;
    }
    std::cout << std::endl;

    std::size_t qid = 0;
    std::string line;
    std::ifstream query_stream(query_file);
    while (std::getline(query_stream, line)) {
        auto query = util::parse_query(line);

        std::vector<bool> hits(index.get_collection_size(), false);
        block_arrays.clear();
        for (std::size_t block_size : block_sizes) {
            std::size_t nblocks =
                calc_nblocks(index.get_collection_size(), block_size);
            block_arrays.emplace_back(nblocks, false);
        }

        for (auto term_weight : query) {
            auto postlist = index.posting_list(term_weight.term);
            for (auto docit = postlist.doc_begin(); docit != postlist.doc_end();
                 ++docit) {
                Doc doc = *docit;
                hits[doc] = true;
                for (std::size_t idx = 0; idx < block_sizes.size(); idx++) {
                    auto block_size = block_sizes[idx];
                    block_arrays[idx][calc_block(doc, block_size)] = true;
                }
            }
        }
        std::size_t hit_count = 0;
        for (bool hit : hits) {
            hit_count += hit;
        }
        std::cout << qid++ << "," << hit_count;
        for (std::size_t idx = 0; idx < block_sizes.size(); idx++) {
            std::size_t skipped = 0;
            for (std::size_t block = 0; block < block_arrays[idx].size();
                 block++) {
                if (!block_arrays[idx][block]) {
                    std::size_t beg = block * block_sizes[idx];
                    std::size_t end = std::min(index.get_collection_size(),
                        (block + 1) * block_sizes[idx]);
                    skipped += end - beg;
                }
            }
            std::cout << "," << skipped;
        }
        std::cout << std::endl;
    }
    return 0;
}

