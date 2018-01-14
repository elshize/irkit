#include <unordered_map>
#include <vector>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#define private public
#define protected public
#include "irkit/index.hpp"
#include "irkit/coding.hpp"

namespace {

using Posting = irkit::_Posting<std::uint16_t, double>;
using IndexT = irkit::Index<std::uint16_t, std::string, std::uint16_t, std::uint16_t>;
using IndexMergerT = irkit::
    IndexMerger<std::uint16_t, std::string, std::uint16_t, std::uint16_t>;

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

irkit::VarByte<std::uint16_t> vb;

class IndexMerging : public ::testing::Test {
protected:
    irkit::fs::path index_dir_1;
    irkit::fs::path index_dir_2;
    irkit::fs::path index_dir_m;
    IndexMerging()
    {
        irkit::fs::path tmpdir = irkit::fs::temp_directory_path();
        index_dir_m = tmpdir / "IndexMergingTest-index_m";
        if (irkit::fs::exists(index_dir_m)) {
            irkit::fs::remove_all(index_dir_m);
        }
        irkit::fs::create_directory(index_dir_m);
        index_dir_1 = tmpdir / "IndexMergingTest-index_1";
        if (irkit::fs::exists(index_dir_1)) {
            irkit::fs::remove_all(index_dir_1);
        }
        irkit::fs::create_directory(index_dir_1);
        write_bytes(irkit::index::terms_path(index_dir_1),
            {'b', '\n', 'c', '\n', 'z', '\n'});
        write_bytes(
            irkit::index::term_doc_freq_path(index_dir_1), vb.encode({2, 1, 1}));
        write_bytes(
            irkit::index::doc_ids_off_path(index_dir_1), vb.encode({0, 2, 1}));
        write_bytes(irkit::index::doc_ids_path(index_dir_1),
            flatten({vb.encode({0, 1}), vb.encode({1}), vb.encode({0})}));
        write_bytes(
            irkit::index::doc_counts_off_path(index_dir_1), vb.encode({0, 2, 1}));
        write_bytes(irkit::index::doc_counts_path(index_dir_1),
            flatten({vb.encode({1, 2}), vb.encode({1}), vb.encode({2})}));
        std::string titles = "Doc1\nDoc2\nDoc3\n";
        std::vector<char> titles_array(titles.begin(), titles.end());
        write_bytes(irkit::index::titles_path(index_dir_1), titles_array);

        index_dir_2 = tmpdir / "IndexMergingTest-index_2";
        if (irkit::fs::exists(index_dir_2)) {
            irkit::fs::remove_all(index_dir_2);
        }
        irkit::fs::create_directory(index_dir_2);
        write_bytes(irkit::index::terms_path(index_dir_2),
            {'b', '\n', 'c', '\n', 'd', '\n'});
        write_bytes(irkit::index::term_doc_freq_path(index_dir_2),
            vb.encode({2, 1, 1}));
        write_bytes(
            irkit::index::doc_ids_off_path(index_dir_2), vb.encode({0, 2, 1}));
        write_bytes(irkit::index::doc_ids_path(index_dir_2),
            flatten({vb.encode({0, 1}), vb.encode({1}), vb.encode({0})}));
        write_bytes(irkit::index::doc_counts_off_path(index_dir_2),
            vb.encode({0, 2, 1}));
        write_bytes(irkit::index::doc_counts_path(index_dir_2),
            flatten({vb.encode({1, 2}), vb.encode({1}), vb.encode({2})}));
        std::string titles_2 = "Doc4\nDoc5\nDoc6\n";
        std::vector<char> titles_array_2(titles_2.begin(), titles_2.end());
        write_bytes(irkit::index::titles_path(index_dir_2), titles_array_2);
    }
    ~IndexMerging() {
        irkit::fs::remove_all(index_dir_1);
        irkit::fs::remove_all(index_dir_2);
        irkit::fs::remove_all(index_dir_m);
    }
    void write_bytes(irkit::fs::path file, const std::vector<char>& bytes)
    {
        std::ofstream ofs(file);
        ofs.write(bytes.data(), bytes.size());
        ofs.close();
    }
};

TEST_F(IndexMerging, titles)
{
    IndexMergerT merger(index_dir_m, {index_dir_1, index_dir_2});
    merger.merge_titles();
    std::ostringstream otitles;
    std::ifstream title_file(irkit::index::titles_path(index_dir_m));
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
    using Posting = irkit::_Posting<std::uint16_t, std::uint16_t>;
    IndexMergerT merger(index_dir_m, {index_dir_1, index_dir_2});
    merger.merge_terms();
    merger.merge_titles();
    IndexT merged(index_dir_m, true);

    // Verify terms
    std::ostringstream oterms;
    std::ifstream term_file(irkit::index::terms_path(index_dir_m));
    std::string term;
    while (std::getline(term_file, term)) {
        oterms << term << std::endl;
    }
    std::string all_terms = oterms.str();
    std::string expected_terms = "b\nc\nd\nz\n";
    ASSERT_EQ(all_terms, expected_terms);

    auto br = merged.posting_range("b", irkit::score::Count{});
    std::vector<Posting> b;
    for (auto p : br) {
        b.push_back(p);
    }
    std::vector<Posting> expected_b = {{0, 1}, {1, 2}, {3, 1}, {4, 2}};
    EXPECT_THAT(b, ::testing::ElementsAreArray(expected_b));

    auto cr = merged.posting_range("c", irkit::score::Count{});
    std::vector<Posting> c;
    for (auto p : cr) {
        c.push_back(p);
    }
    std::vector<Posting> expected_c = {{1, 1}, {4, 1}};
    EXPECT_THAT(c, ::testing::ElementsAreArray(expected_c));

    auto dr = merged.posting_range("d", irkit::score::Count{});
    std::vector<Posting> d;
    for (auto p : dr) {
        d.push_back(p);
    }
    std::vector<Posting> expected_d = {{3, 2}};
    EXPECT_THAT(d, ::testing::ElementsAreArray(expected_d));

    auto zr = merged.posting_range("z", irkit::score::Count{});
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
