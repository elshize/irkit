#include "run.hpp"
#include "irkit/taat.hpp"

using namespace bloodhound;

std::vector<query::Result> range_taat(const std::vector<PostingList>& postings,
    const std::vector<Score>& weights,
    std::size_t k,
    index::Index<>& ind)
{
    std::vector<Posting> top =
        irk::taat(postings, k, weights, ind.get_collection_size());
    return to_results(top);
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cerr << "usage: runtaat <index_dir> <query_file>\n";
        exit(1);
    }
    fs::path index_dir(argv[1]);
    fs::path query_file(argv[2]);

    fs::path titles_file = index_dir / "titles";
    std::vector<std::string> titles = load_titles(titles_file);

    index::Index index = index::Index<>::load_index(index_dir);
    run_with(range_taat, index, query_file);

}
