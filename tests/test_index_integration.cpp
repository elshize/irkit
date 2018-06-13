#include <unordered_map>
#include <vector>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#define private public
#define protected public
#include <irkit/coding/varbyte.hpp>
#include <irkit/index.hpp>
#include <irkit/index/builder.hpp>
#include <irkit/index/assembler.hpp>

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

void test_postings(const irk::v2::inverted_index_view& index_view,
        int term_id, std::vector<std::pair<long, long>> expected)
{
    auto postings = index_view.postings(term_id);
    EXPECT_EQ(postings.size(), expected.size());
    std::vector<std::pair<long, long>> v(postings.begin(), postings.end());
    ASSERT_THAT(v, ::testing::ElementsAreArray(expected));
}

TEST(IndexIntegration, build_write_read)
{
    // given
    auto index_dir = irk::fs::temp_directory_path() / "IndexIntegrationTest";
    if (irk::fs::exists(index_dir)) {
        irk::fs::remove_all(index_dir);
    }
    irk::fs::create_directory(index_dir);
    std::istringstream input("Doc1\ta b a\nDoc2\tc b b\nDoc3\tz c a\n");

    // when
    irk::index::index_assembler<long, std::string, long, long> assembler(
        fs::path(index_dir),
        2,
        1024,
        irk::coding::varbyte_codec<long>{},
        irk::coding::varbyte_codec<long>{});

    assembler.assemble(input);
    auto term_map = irk::build_prefix_map_from_file<long>(
        irk::index::terms_path(index_dir));
    irk::io::dump(term_map, irk::index::term_map_path(index_dir));
    auto title_map = irk::build_prefix_map_from_file<long>(
        irk::index::titles_path(index_dir));
    irk::io::dump(title_map, irk::index::title_map_path(index_dir));

    // then
    auto data = std::make_shared<irk::v2::inverted_index_mapped_data_source>(
        index_dir);
    irk::v2::inverted_index_view index_view(data.get(),
        irk::coding::varbyte_codec<long>{},
        irk::coding::varbyte_codec<long>{});

    ASSERT_EQ(index_view.term_id("a"), 0);
    ASSERT_EQ(index_view.term_id("b"), 1);
    ASSERT_EQ(index_view.term_id("c"), 2);
    ASSERT_EQ(index_view.term_id("z"), 3);

    test_postings(index_view, 0, {{0, 2}, {2, 1}});
    test_postings(index_view, 1, {{0, 1}, {1, 2}});
    test_postings(index_view, 2, {{1, 1}, {2, 1}});
    test_postings(index_view, 3, {{2, 1}});
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
