#include <algorithm>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <irkit/index.hpp>
#include <irkit/index/assembler.hpp>
#include <irkit/index/score.hpp>
#include <irkit/index/source.hpp>
#include <irkit/index/types.hpp>
#include <irkit/quantize.hpp>

namespace {

namespace fs = boost::filesystem;
namespace ts = type_safe;

using posting_type = std::pair<irk::index::document_t, irk::index::frequency_t>;
using posting_map = std::map<std::string, std::vector<posting_type>>;
using irk::index::document_t;
using irk::index::frequency_t;

struct on_fly_index {
    posting_map freq_postings;
    posting_map scored_postings;
    std::vector<long> document_sizes;
    std::map<std::string, long> occurrences;
    long collection_occurrences;
    long collection_size;
};

auto scored(const posting_map& postings,
    const std::vector<long>& document_sizes,
    std::map<std::string, long> occurrences,
    long collection_occurrences,
    int collection_size,
    int bits)
{
    long max_document_size =
        *std::max_element(document_sizes.begin(), document_sizes.end());
    std::map<std::string, std::vector<posting_type>> scored;
    double max_score = 0;
    for (const auto& [term, posting_for_term] : postings)
    {
        irk::score::query_likelihood_scorer scorer(
            occurrences[term], collection_occurrences, max_document_size);
        for (const auto& [doc, freq] : posting_for_term)
        {
            max_score = std::max(max_score, scorer(freq, document_sizes[doc]));
        }
    }
    long max_int = (1 << bits) - 1;
    for (const auto& [term, posting_for_term] : postings)
    {
        std::vector<posting_type> scored_for_term;
        irk::score::query_likelihood_scorer scorer(
            occurrences[term], collection_occurrences, max_document_size);
        auto quantize = irk::LinearQuantizer{
            irk::RealRange{0, max_score}, irk::IntegralRange{1, max_int}};
        for (const auto& [doc, freq] : posting_for_term) {
            double score = scorer(freq, document_sizes[doc]);
            long quantized_score = quantize(score);
            scored_for_term.push_back({doc, quantized_score});
        }
        scored[term] = std::move(scored_for_term);
    }
    return scored;
}

on_fly_index postings_on_fly(fs::path collection_file, int bits)
{
    std::map<std::string, std::vector<posting_type>> postings;
    std::map<std::string,
        std::map<irk::index::document_t, irk::index::frequency_t>>
        postings_map;
    std::vector<long> document_sizes;
    std::map<std::string, long> occurrences;
    std::ifstream input(collection_file.c_str());
    std::string line;
    irk::index::document_t doc(0);
    long collection_occurrences = 0;
    while (std::getline(input, line)) {
        std::istringstream line_input(line);
        std::string title, term;
        line_input >> title;
        long doc_size = 0;
        while (line_input >> term) {
            postings_map[term][doc]++;
            occurrences[term]++;
            collection_occurrences++;
            doc_size++;
        }
        document_sizes.push_back(doc_size);
        doc++;
    }
    long collection_size = doc;
    for (const auto& [term, map] : postings_map)
    {
        for (const auto& [id, count] : map) {
            postings[term].push_back({id, count});
        }
        std::sort(postings[term].begin(), postings[term].end());
    }
    return {postings,
        scored(postings,
            document_sizes,
            occurrences,
            collection_occurrences,
            collection_size,
            bits),
        document_sizes,
        occurrences,
        collection_occurrences,
        collection_size};
}

class inverted_index : public ::testing::Test {
protected:
    boost::filesystem::path index_dir;
    std::string collection_file;
    on_fly_index expected_index;

    inverted_index() {
        index_dir = irk::fs::temp_directory_path()
            / "irkit-inverted_index";
        if (irk::fs::exists(index_dir)) { irk::fs::remove_all(index_dir); }
        irk::fs::create_directory(index_dir);

        // given
        collection_file = "collection.txt";
        irk::io::enforce_exist(collection_file);
        expected_index = postings_on_fly(collection_file, 8);
        irk::index::index_assembler assembler(
            fs::path(index_dir), 32, 1024, 16);

        std::ifstream input(collection_file);
        assembler.assemble(input);
        irk::index::score_index<
            irk::score::query_likelihood_scorer,
            irk::inverted_index_inmemory_data_source>(index_dir, 8);
    }
};

template<class D, class F, class S>
void test(const irk::basic_inverted_index_view<D, F, S>& index_view,
    on_fly_index& expected_index)
{
    ASSERT_EQ(index_view.collection_size(), 100);
    ASSERT_EQ(index_view.occurrences_count(), expected_index.collection_occurrences);
    for (long doc = 0; doc < index_view.collection_size(); doc++) {
        ASSERT_EQ(index_view.document_size(irk::index::document_t(doc)),
            expected_index.document_sizes[doc]);
    }

    for (long term_id = 0; term_id < index_view.terms().size(); term_id++) {
        std::string term = index_view.term(term_id);
        ASSERT_EQ(index_view.term_occurrences(term_id),
            expected_index.occurrences[term]);
        auto postings = index_view.postings(term_id);
        const auto& expected =
            expected_index.freq_postings[index_view.term(term_id)];
        std::vector<posting_type> actual;
        for (const auto& posting : postings) {
            actual.emplace_back(posting.document(), posting.payload());
        }
        ASSERT_THAT(actual, ::testing::ElementsAreArray(expected));
        auto scored_postings = index_view.scored_postings(term_id);
        const auto& expected_scored =
            expected_index.scored_postings[index_view.term(term_id)];
        std::vector<posting_type> actual_scored;
        for (const auto& posting : scored_postings) {
            actual_scored.emplace_back(posting.document(), posting.payload());
        }
        ASSERT_THAT(
            actual_scored, ::testing::ElementsAreArray(expected_scored));
    }
}

TEST_F(inverted_index, mapped_file)
{
    // when
    auto data =
        irk::inverted_index_mapped_data_source::from(index_dir, {"ql"}).value();
    irk::inverted_index_view index_view(&data);
    // then
    test(index_view, expected_index);
}

}  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
