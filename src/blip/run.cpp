#include "irkit/utils.hpp"
#include "retrievers.hpp"

#include "run.hpp"

using namespace bloodhound;

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

std::vector<query::Result> to_results(std::vector<Posting>& postings)
{
    std::vector<query::Result> results;
    for (auto& [doc, score] : postings) {
        results.push_back({doc, score});
    }
    return results;
}

void run_with(std::vector<query::Result> (*run)(const std::vector<PostingList>&,
                  const std::vector<Score>&,
                  std::size_t, index::Index<>&),
    index::Index<>& ind,
    fs::path query_file)
{
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
                auto posting_list = ind.posting_list(termid);
                query_posting_lists.push_back(posting_list);
                term_weights.push_back(weight);
            }

            std::size_t k = 30;

            auto start_interval = std::chrono::steady_clock::now();

            std::vector<query::Result> top_results =
                run(query_posting_lists, term_weights, k, ind);

            auto end_interval = std::chrono::steady_clock::now();
            elapsed += std::chrono::duration_cast<std::chrono::nanoseconds>(
                end_interval - start_interval);

            std::cout << "Query " << query_count << "("
                      << query_posting_lists.size() << " terms); Found "
                      << top_results.size() << " top results.\n";
            for (const auto& result : top_results) {
                std::cout << "Doc: " << result.doc
                          << ", Score: " << result.score
                          << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception occurred while processing query "
                      << query_count << std::endl;
            std::cerr << e.what() << std::endl;
        }

        query_count++;
    }
    std::cerr << "Average time: "
              << std::chrono::duration_cast<std::chrono::microseconds>(
                     elapsed)
                     .count()
            / query_count
              << std::endl;
}
