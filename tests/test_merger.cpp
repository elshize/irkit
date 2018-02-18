#include <unordered_map>
#include <vector>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#define private public
#define protected public
#include "irkit/coding.hpp"
#include "irkit/coding/varbyte.hpp"
#include "irkit/index.hpp"
#include "irkit/index/merger.hpp"

namespace {

using Posting = irk::_posting<std::uint16_t, double>;
using IndexT = irk::
    inverted_index<std::uint16_t, std::string, std::uint16_t, std::uint16_t>;
using index_merger =
    irk::index_merger<std::uint16_t, std::string, std::uint16_t, std::uint16_t>;

struct FakeScore {
    template<class Freq>
    double operator()(Freq tf, Freq df, std::size_t collection_size) const
    {
        return tf;
    }
};

std::vector<char> flatten(std::vector<std::vector<char>> vectors)
{
    std::vector<char> result;
    for (std::vector<char>& vec : vectors) {
        result.insert(result.end(), vec.begin(), vec.end());
    }
    return result;
}

irk::coding::varbyte_codec<std::uint16_t> vb;

auto vb_encode(std::initializer_list<std::uint16_t> integers)
{
    return irk::coding::encode(integers, vb);
}

class IndexMerging : public ::testing::Test {
protected:
    irk::fs::path index_dir_1;
    irk::fs::path index_dir_2;
    irk::fs::path index_dir_m;
    IndexMerging()
    {
        irk::fs::path tmpdir = irk::fs::temp_directory_path();
        index_dir_m = tmpdir / "IndexMergingTest-index_m";
        if (irk::fs::exists(index_dir_m)) {
            irk::fs::remove_all(index_dir_m);
        }
        irk::fs::create_directory(index_dir_m);
        index_dir_1 = tmpdir / "IndexMergingTest-index_1";
        if (irk::fs::exists(index_dir_1)) {
            irk::fs::remove_all(index_dir_1);
        }
        irk::fs::create_directory(index_dir_1);
        write_bytes(irk::index::terms_path(index_dir_1),
            {'b', '\n', 'c', '\n', 'z', '\n'});
        write_bytes(irk::index::term_doc_freq_path(index_dir_1),
            vb_encode({2, 1, 1}));
        irk::io::dump(irk::offset_table<>({0, 2, 3}),
            irk::index::doc_ids_off_path(index_dir_1));
        write_bytes(irk::index::doc_ids_path(index_dir_1),
            flatten({vb_encode({0, 1}), vb_encode({1}), vb_encode({0})}));
        irk::io::dump(irk::offset_table<>({0, 2, 3}),
            irk::index::doc_counts_off_path(index_dir_1));
        write_bytes(irk::index::doc_counts_path(index_dir_1),
            flatten({vb_encode({1, 2}), vb_encode({1}), vb_encode({2})}));
        std::string titles = "Doc1\nDoc2\nDoc3\n";
        std::vector<char> titles_array(titles.begin(), titles.end());
        write_bytes(irk::index::titles_path(index_dir_1), titles_array);

        index_dir_2 = tmpdir / "IndexMergingTest-index_2";
        if (irk::fs::exists(index_dir_2)) {
            irk::fs::remove_all(index_dir_2);
        }
        irk::fs::create_directory(index_dir_2);
        write_bytes(irk::index::terms_path(index_dir_2),
            {'b', '\n', 'c', '\n', 'd', '\n'});
        write_bytes(irk::index::term_doc_freq_path(index_dir_2),
            vb_encode({2, 1, 1}));
        irk::io::dump(irk::offset_table<>({0, 2, 3}),
            irk::index::doc_ids_off_path(index_dir_2));
        write_bytes(irk::index::doc_ids_path(index_dir_2),
            flatten({vb_encode({0, 1}), vb_encode({1}), vb_encode({0})}));
        irk::io::dump(irk::offset_table<>({0, 2, 3}),
            irk::index::doc_counts_off_path(index_dir_2));
        write_bytes(irk::index::doc_counts_path(index_dir_2),
            flatten({vb_encode({1, 2}), vb_encode({1}), vb_encode({2})}));
        std::string titles_2 = "Doc4\nDoc5\nDoc6\n";
        std::vector<char> titles_array_2(titles_2.begin(), titles_2.end());
        write_bytes(irk::index::titles_path(index_dir_2), titles_array_2);
    }
    ~IndexMerging() {
        irk::fs::remove_all(index_dir_1);
        irk::fs::remove_all(index_dir_2);
        irk::fs::remove_all(index_dir_m);
    }
    void write_bytes(irk::fs::path file, const std::vector<char>& bytes)
    {
        std::ofstream ofs(file);
        ofs.write(bytes.data(), bytes.size());
        ofs.close();
    }
};

TEST_F(IndexMerging, titles)
{
    index_merger merger(index_dir_m, {index_dir_1, index_dir_2});
    merger.merge_titles();
    std::ostringstream otitles;
    std::ifstream title_file(irk::index::titles_path(index_dir_m));
    std::string title;
    while (std::getline(title_file, title)) {
        otitles << title << std::endl;
    }
    std::string all_titles = otitles.str();
    std::string expected = "Doc1\nDoc2\nDoc3\nDoc4\nDoc5\nDoc6\n";
    ASSERT_EQ(all_titles, expected);
}

TEST_F(IndexMerging, merge_terms)
{
    using Posting = irk::_posting<std::uint16_t, std::uint16_t>;
    index_merger merger(index_dir_m, {index_dir_1, index_dir_2});
    merger.merge_terms();
    merger.merge_titles();
    IndexT merged(index_dir_m, true);

    // Verify terms
    std::ostringstream oterms;
    std::ifstream term_file(irk::index::terms_path(index_dir_m));
    std::string term;
    while (std::getline(term_file, term)) {
        oterms << term << std::endl;
    }
    std::string all_terms = oterms.str();
    std::string expected_terms = "b\nc\nd\nz\n";
    ASSERT_EQ(all_terms, expected_terms);

    auto br = merged.posting_range("b", irk::score::count_scorer{});
    std::vector<Posting> b;
    for (auto p : br) {
        b.push_back(p);
    }
    std::vector<Posting> expected_b = {{0, 1}, {1, 2}, {3, 1}, {4, 2}};
    EXPECT_THAT(b, ::testing::ElementsAreArray(expected_b));

    auto cr = merged.posting_range("c", irk::score::count_scorer{});
    std::vector<Posting> c;
    for (auto p : cr) {
        c.push_back(p);
    }
    std::vector<Posting> expected_c = {{1, 1}, {4, 1}};
    EXPECT_THAT(c, ::testing::ElementsAreArray(expected_c));

    auto dr = merged.posting_range("d", irk::score::count_scorer{});
    std::vector<Posting> d;
    for (auto p : dr) {
        d.push_back(p);
    }
    std::vector<Posting> expected_d = {{3, 2}};
    EXPECT_THAT(d, ::testing::ElementsAreArray(expected_d));

    auto zr = merged.posting_range("z", irk::score::count_scorer{});
    std::vector<Posting> z;
    for (auto p : zr) {
        z.push_back(p);
    }
    std::vector<Posting> expected_z = {{0, 2}};
    EXPECT_THAT(z, ::testing::ElementsAreArray(expected_z));
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
