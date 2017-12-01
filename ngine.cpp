#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <unordered_map>
#define STATS
#include "daat.hpp"
#include "index.hpp"
#include "query.hpp"
#include "taat.hpp"

using namespace ngine;

std::vector<std::tuple<TermId, Score>> parse_query(
    std::string& query_line)
{
    std::vector<std::tuple<TermId, Score>> query;
    std::string term_pair;
    std::istringstream line_stream(query_line);
    while (line_stream >> term_pair) {
        std::size_t pos = term_pair.find(":");
        term_pair[pos] = ' ';
        std::istringstream pair_stream(term_pair);
        TermId termid;
        Score score;
        pair_stream >> termid;
        pair_stream >> score;
        query.push_back(std::make_tuple(termid, score));
    }
    return query;
}

std::vector<std::string> load_titles(fs::path titles_path)
{
    std::vector<std::string> titles;
    std::string title;
    std::ifstream title_stream(titles_path);
    while (std::getline(title_stream, title)) {
        titles.push_back(title);
    }
    return titles;
}

int main(int argc, char** argv)
{
    if (argc != 4) {
        std::cerr << "usage: ngine {taat[flags]|daat|wand} <index_dir> <query_file>\n";
        exit(1);
    }
    std::string type(argv[1]);
    fs::path index_dir(argv[2]);
    fs::path query_file(argv[3]);
    fs::path titles_file = index_dir / "titles";
    std::vector<std::string> titles = load_titles(titles_file);

    index::Index index = index::Index::load_index(index_dir);

    //query::TaatProcessor<true, 8, 1024> taat_plus(
    query::TaatRetriever<PostingList, false, 8, 0> taat_plus(
        index.get_collection_size());
    query::TaatRetriever<PostingList> taat(index.get_collection_size());
    query::DaatProcessor daat;
    query::WandProcessor wand;

    //std::size_t postings = 0;
    std::size_t query_count = 0;
    std::chrono::nanoseconds elapsed(0);
    std::string line;
    std::ifstream query_stream(query_file);
    while (std::getline(query_stream, line)) {
        try {
            auto query_terms_weights = parse_query(line);
            std::vector<PostingList> query_posting_lists;
            std::vector<Score> term_weights;
            for (const auto & [ termid, weight ] : query_terms_weights) {
                if (weight == Score(0))
                    continue;
                auto posting_list = index.posting_list(termid);
                query_posting_lists.push_back(posting_list);
                term_weights.push_back(weight);
            }

            std::vector<query::Result> top_results;
            std::size_t k = 30;

            auto start_interval = std::chrono::steady_clock::now();

            if (type.compare(0, 4, "taat") == 0) {
                if (type.find("+") != std::string::npos) {
                    top_results = taat_plus.retrieve(query_posting_lists, term_weights, k);
                } else {
                    top_results = taat.retrieve(query_posting_lists, term_weights, k);
                }
            } else if (type == "daat") {
                top_results = daat.process(query_posting_lists, term_weights, k);
            } else if (type == "wand") {
                top_results = wand.process(query_posting_lists, term_weights, k);
            } else {
                std::cerr << "Type of query processing `" << type
                          << "` is not supported.\n";
                exit(1);
            }

            auto end_interval = std::chrono::steady_clock::now();
            elapsed += std::chrono::duration_cast<std::chrono::nanoseconds>(
                end_interval - start_interval);

            std::cout << "Query " << query_count
                      << "; Found " << top_results.size() << " top results.\n";
            for (const auto& result : top_results) {
                std::cout << "Doc: " << result.doc
                          << ", Score: " << result.score
                          << std::endl;
            }
        } catch (...) {
            std::cerr << "Exception occurred while processing query "
                      << query_count << std::endl;
        }

        query_count++;
    }
    //std::cout << "Avg postings: " << postings / query_count << std::endl;
    std::cerr << "Average time: "
              << std::chrono::duration_cast<std::chrono::microseconds>(
                     elapsed)
                     .count()
            / query_count
              << std::endl;
}
