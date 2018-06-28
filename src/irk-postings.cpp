#include <iostream>

#include <CLI/CLI.hpp>
#include <boost/filesystem.hpp>

#include <irkit/coding/varbyte.hpp>
#include <irkit/index.hpp>
#include <irkit/index/types.hpp>
#include <irkit/index/source.hpp>

namespace fs = boost::filesystem;

template<class PostingListT>
void print_postings(const PostingListT& postings)
{
    for (const auto& posting : postings)
    { std::cout << posting.document() << "\t" << posting.payload() << "\n"; }
}

int main(int argc, char** argv)
{
    std::string dir = ".";
    std::string term;
    std::string scoring;
    bool use_id = false;

    CLI::App app{"Prints postings."
                 "Fist column: document IDs. Second column: payload."};
    app.add_option("-d,--index-dir", dir, "index directory", true)
        ->check(CLI::ExistingDirectory);
    app.add_flag("-i,--use-id", use_id, "use a term ID");
    app.add_option("-s,--scores",
        scoring,
        "print given scores instead of frequencies",
        true);
    app.add_option("term", term, "term to look up", false)->required();

    CLI11_PARSE(app, argc, argv);

    bool scores_defined = app.count("--scores") > 0;

    try {
        irk::inverted_index_disk_data_source data(fs::path{dir},
            scores_defined ? std::make_optional(scoring) : std::nullopt);
        irk::inverted_index_view index(&data,
            irk::varbyte_codec<irk::index::document_t>{},
            irk::varbyte_codec<long>{},
            irk::varbyte_codec<long>{});

        long term_id = use_id ? std::stol(term) : *index.term_id(term);
        if (app.count("--scores") > 0) {
            print_postings(index.scored_postings(term_id));
        } else {
            print_postings(index.postings(term_id));
        }
    } catch (std::bad_optional_access e) {
        std::cerr << "Term " << term << " not found." << std::endl;
    }

}
