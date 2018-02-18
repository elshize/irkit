#include <unordered_map>
#include <vector>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#define private public
#define protected public
#include "irkit/coding/varbyte.hpp"
#include "irkit/index.hpp"
#include "irkit/index/builder.hpp"

namespace {

struct FakeScore {
    template<class Freq>
    double operator()(Freq tf, Freq df, std::size_t collection_size) const
    {
        return tf;
    }
};

using Posting = irk::_posting<uint32_t, double>;

void test_load_read(irk::fs::path index_dir, bool in_memory)
{
    // Load
    irk::default_index index(index_dir, in_memory);

    // Test Postings: "a"
    auto a = index.posting_range("a", FakeScore{});
    std::vector<Posting> ap;
    for (auto posting : a) {
        ap.push_back(posting);
    }
    std::vector<Posting> exp_ap = {{0, 2}, {2, 1}};
    EXPECT_THAT(ap, ::testing::ElementsAreArray(exp_ap));

    // Test Postings: "b"
    auto b = index.posting_range("b", FakeScore{});
    std::vector<Posting> bp;
    for (auto posting : b) {
        bp.push_back(posting);
    }
    std::vector<Posting> exp_bp = {{0, 1}, {1, 2}};
    EXPECT_THAT(bp, ::testing::ElementsAreArray(exp_bp));

    // Test Postings: "c"
    auto c = index.posting_range("c", FakeScore{});
    std::vector<Posting> cp;
    for (auto posting : c) {
        cp.push_back(posting);
    }
    std::vector<Posting> exp_cp = {{1, 1}, {2, 1}};
    EXPECT_THAT(cp, ::testing::ElementsAreArray(exp_cp));

    // Test Postings: "z"
    auto z = index.posting_range("z", FakeScore{});
    std::vector<Posting> zp;
    for (auto posting : z) {
        zp.push_back(posting);
    }
    std::vector<Posting> exp_zp = {{2, 1}};
    EXPECT_THAT(zp, ::testing::ElementsAreArray(exp_zp));
}

TEST(IndexIntegration, build_write_read)
{
    auto index_dir = irk::fs::temp_directory_path() / "IndexIntegrationTest";
    if (irk::fs::exists(index_dir)) {
        irk::fs::remove_all(index_dir);
    }
    irk::fs::create_directory(index_dir);

    // Build
    irk::default_index_builder builder;
    builder.add_term("a");
    builder.add_term("b");
    builder.add_term("a");
    builder.add_document();
    builder.add_term("c");
    builder.add_term("b");
    builder.add_term("b");
    builder.add_document();
    builder.add_term("z");
    builder.add_term("c");
    builder.add_term("a");

    // Write
    std::ofstream of_doc_ids(irk::index::doc_ids_path(index_dir));
    std::ofstream of_doc_ids_off(irk::index::doc_ids_off_path(index_dir));
    std::ofstream of_doc_counts(irk::index::doc_counts_path(index_dir));
    std::ofstream of_doc_counts_off(irk::index::doc_counts_off_path(index_dir));
    std::ofstream of_terms(irk::index::terms_path(index_dir));
    std::ofstream of_term_doc_freq(irk::index::term_doc_freq_path(index_dir));
    std::ofstream of_titles(irk::index::titles_path(index_dir));
    builder.sort_terms();
    builder.write_terms(of_terms);
    builder.write_document_frequencies(of_term_doc_freq);
    builder.write_document_ids(of_doc_ids, of_doc_ids_off);
    builder.write_document_counts(of_doc_counts, of_doc_counts_off);
    of_titles << "Doc1\nDoc2\nDoc3\n";
    of_doc_ids.close();
    of_doc_ids_off.close();
    of_doc_counts.close();
    of_doc_counts_off.close();
    of_terms.close();
    of_term_doc_freq.close();
    of_titles.close();

    test_load_read(index_dir, true);
    test_load_read(index_dir, false);
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
