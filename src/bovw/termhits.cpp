#include <algorithm>
#include <cmath>
#include <iostream>
#include "index.hpp"
#include "query.hpp"
#include "irkit/taat.hpp"
#include "type_safe/strong_typedef.hpp"

using namespace bloodhound;

std::tuple<std::vector<PostingList>, std::vector<Score>> parse_query(
    const std::string& query_line,
    index::Index<>& index)
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
    std::vector<PostingList> query_posting_lists;
    std::vector<Score> term_weights;
    for (const auto & [termid, weight] : query) {
        if (weight == Score(0))
            continue;
        auto posting_list = index.posting_list(termid);
        query_posting_lists.push_back(posting_list);
        term_weights.push_back(weight);
    }
    return std::make_tuple(query_posting_lists, term_weights);
}

struct DocCountingAccumulator {
    std::vector<Score> scores;
    std::vector<std::size_t> counts;
    explicit DocCountingAccumulator(std::size_t size)
        : scores(size, Score(0)), counts(size, 0)
    {}
};

class DocCountingAccumulatorPolicy {
public:
    void
    accumulate_posting(Doc doc, Score score_delta, DocCountingAccumulator& acc)
    {
        acc.scores[doc] += score_delta;
        acc.counts[doc] += 1;
    }
};

int main(int argc, char** argv)
{
    if (argc != 4) {
        std::cerr << "usage: termhits <index_dir> <query_file> <k>\n";
        exit(1);
    }

    fs::path query_file(argv[2]);
    fs::path index_dir(argv[1]);
    index::Index index = index::Index<>::load_index(index_dir);

    std::size_t k = std::stoi(argv[3]);

    std::cout << "query\trank\tdoc\tquery_terms\tterm_hits" << std::endl;

    std::size_t qid = 0;
    std::string line;
    std::ifstream query_stream(query_file.c_str());
    while (std::getline(query_stream, line)) {
        auto[postings, term_weights] = parse_query(line, index);
        DocCountingAccumulator acc(index.get_collection_size());
        irk::traverse_postings(
            postings, acc, term_weights, DocCountingAccumulatorPolicy{});
        auto top = irk::aggregate_top<query::Result>(k, acc.scores);
        std::size_t rank = 0;
        for (auto result : top) {
            auto doc = result.doc;
            auto term_hits = acc.counts[doc];
            std::cout << qid
                << "\t" << rank++
                << "\t" << doc
                << "\t" << postings.size()
                << "\t" << term_hits
                << std::endl;
        }
        qid++;
    }
    return 0;
}

