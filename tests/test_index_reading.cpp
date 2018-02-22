#include <unordered_map>
#include <vector>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#define private public
#define protected public
#include "irkit/coding/varbyte.hpp"
#include "irkit/index.hpp"
#include "irkit/io.hpp"

namespace {

using Posting = irk::_posting<std::uint16_t, double>;
using IndexT = irk::
    inverted_index<std::uint16_t, std::string, std::uint16_t, std::uint16_t>;

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

irk::mapped_compact_table<std::uint16_t>
mct(std::initializer_list<std::uint16_t> numbers, fs::path file)
{
    auto ct = irk::build_compact_table<std::uint16_t>(numbers);
    irk::io::dump(ct, file);
    return irk::map_compact_table<std::uint16_t>(file);
}

irk::mapped_offset_table<>
mot(std::initializer_list<std::size_t> numbers, fs::path file)
{
    auto ct = irk::build_offset_table<>(numbers);
    irk::io::dump(ct, file);
    return irk::map_offset_table<>(file);
}

class IndexReading : public ::testing::Test {
protected:
    using index_type = irk::inverted_index<std::uint16_t,
        std::string,
        std::uint16_t,
        std::uint16_t>;
    std::unique_ptr<index_type> index;
    IndexReading()
    {
        fs::path tmpdir =
            irk::fs::temp_directory_path() / "IndexReadingTest";
        if (irk::fs::exists(tmpdir)) {
            irk::fs::remove_all(tmpdir);
        }
        irk::fs::create_directory(tmpdir);
        index.reset(new index_type({"b", "c", "z"},  // terms_
            mct({2, 1, 1}, tmpdir / "termdfs"),  // term_dfs_
            flatten({vb_encode({0, 1}),
                vb_encode({1}),
                vb_encode({0})}),  // doc_ids_
            mot({0, 2, 3}, tmpdir / "doc_ids_off_"),  // doc_ids_off_
            flatten({vb_encode({1, 2}),
                vb_encode({1}),
                vb_encode({2})}),  // doc_counts_
            mot({0, 2, 3}, tmpdir / "doc_counts_off_"),  // doc_counts_off_
            {"Doc1", "Doc2", "Doc3"}));
    }
};


TEST_F(IndexReading, offsets)
{
    EXPECT_EQ(index->offset("b", index->doc_ids_off_), 0);
    EXPECT_EQ(index->offset(0, index->doc_ids_off_), 0);
    EXPECT_EQ(index->offset("c", index->doc_ids_off_), 2);
    EXPECT_EQ(index->offset(1, index->doc_ids_off_), 2);
    EXPECT_EQ(index->offset("z", index->doc_ids_off_), 3);
    EXPECT_EQ(index->offset(2, index->doc_ids_off_), 3);
    EXPECT_EQ(index->offset("b", index->doc_counts_off_), 0);
    EXPECT_EQ(index->offset(0, index->doc_counts_off_), 0);
    EXPECT_EQ(index->offset("c", index->doc_counts_off_), 2);
    EXPECT_EQ(index->offset(1, index->doc_counts_off_), 2);
    EXPECT_EQ(index->offset("z", index->doc_counts_off_), 3);
    EXPECT_EQ(index->offset(2, index->doc_counts_off_), 3);
}

TEST_F(IndexReading, posting_range)
{
    auto bystring = index->posting_range("b", FakeScore{});
    auto byid = index->posting_range(0, FakeScore{});
    std::vector<Posting> bystring_actual;
    std::vector<Posting> byid_actual;
    std::vector<Posting> expected = {{0, 1.0}, {1, 2.0}};
    for (const auto& posting : bystring) {
        bystring_actual.push_back(posting);
    }
    for (const auto& posting : byid) {
        byid_actual.push_back(posting);
    }
    EXPECT_THAT(bystring_actual, ::testing::ElementsAreArray(expected));
    EXPECT_THAT(byid_actual, ::testing::ElementsAreArray(expected));
}


class IndexLoading : public ::testing::Test {
protected:
    irk::fs::path index_dir;
    std::unique_ptr<IndexT> index;
    IndexLoading()
    {
        index_dir = irk::fs::temp_directory_path() / "IndexLoadingTest";
        if (irk::fs::exists(index_dir)) {
            irk::fs::remove_all(index_dir);
        }
        irk::fs::create_directory(index_dir);
        write_bytes(irk::index::terms_path(index_dir),
            {'b', '\n', 'c', '\n', 'z', '\n'});
        auto tdfs = irk::build_compact_table<std::uint16_t>({2, 1, 1});
        irk::io::dump(tdfs, irk::index::term_doc_freq_path(index_dir));
        irk::io::dump(irk::build_offset_table<>({0, 2, 3}),
            irk::index::doc_ids_off_path(index_dir));
        write_bytes(irk::index::doc_ids_path(index_dir),
            flatten({vb_encode({0, 1}), vb_encode({1}), vb_encode({0})}));
        irk::io::dump(irk::build_offset_table<>({0, 2, 3}),
            irk::index::doc_counts_off_path(index_dir));
        write_bytes(irk::index::doc_counts_path(index_dir),
            flatten({vb_encode({1, 2}), vb_encode({1}), vb_encode({2})}));
        std::string titles = "Doc1\nDoc2\nDoc3\n";
        std::vector<char> titles_array(titles.begin(), titles.end());
        write_bytes(irk::index::titles_path(index_dir), titles_array);
        index.reset(new IndexT(index_dir));
    }
    ~IndexLoading() { irk::fs::remove_all(index_dir); }
    void write_bytes(irk::fs::path file, const std::vector<char>& bytes)
    {
        std::ofstream ofs(file.c_str());
        ofs.write(bytes.data(), bytes.size());
        ofs.close();
    }
};

TEST_F(IndexLoading, load)
{
    EXPECT_EQ(index->collection_size(), 3);

    std::string e_terms = "bcz";
    std::string actual_terms;
    for (const auto& term : index->terms_) {
        actual_terms.append(*term);
    }
    EXPECT_EQ(actual_terms, e_terms);

    std::vector<std::pair<std::string, std::uint16_t>> e_term_map = {
        {"b", 0}, {"c", 1}, {"z", 2}};
    std::vector<std::pair<std::string, std::uint16_t>> a_term_map;
    for (const auto& term : index->terms_) {
        a_term_map.push_back({*term, index->term_map_[term.get()]});
    }
    std::sort(a_term_map.begin(), a_term_map.end());
    EXPECT_THAT(a_term_map, ::testing::ElementsAreArray(e_term_map));

    std::vector<std::uint16_t> e_term_dfs = {2, 1, 1};
    for (std::size_t idx = 0; idx < index->term_dfs_.size(); ++idx) {
        EXPECT_THAT(index->term_dfs_[idx], e_term_dfs[idx]);
    }

    std::vector<char> e_doc_ids =
        flatten({vb_encode({0, 1}), vb_encode({1}), vb_encode({0})});
    EXPECT_THAT(index->doc_ids_, ::testing::ElementsAreArray(e_doc_ids));

    std::vector<char> e_doc_counts =
        flatten({vb_encode({1, 2}), vb_encode({1}), vb_encode({2})});
    EXPECT_THAT(index->doc_counts_, ::testing::ElementsAreArray(e_doc_counts));

    std::vector<std::uint16_t> e_doc_ids_off = {0, 2, 3};
    for (std::size_t idx = 0; idx < index->doc_ids_off_.size(); ++idx) {
        EXPECT_THAT(index->doc_ids_off_[idx], e_doc_ids_off[idx]);
    }

    std::vector<std::uint16_t> e_doc_counts_off = {0, 2, 3};
    for (std::size_t idx = 0; idx < index->doc_counts_off_.size(); ++idx) {
        EXPECT_THAT(index->doc_counts_off_[idx], e_doc_counts_off[idx]);
    }
}

TEST_F(IndexLoading, offset)
{
    EXPECT_EQ(index->offset("b", index->doc_ids_off_), 0);
    EXPECT_EQ(index->offset(0, index->doc_ids_off_), 0);
    EXPECT_EQ(index->offset("c", index->doc_ids_off_), 2);
    EXPECT_EQ(index->offset(1, index->doc_ids_off_), 2);
    EXPECT_EQ(index->offset("z", index->doc_ids_off_), 3);
    EXPECT_EQ(index->offset(2, index->doc_ids_off_), 3);
    EXPECT_EQ(index->offset("b", index->doc_counts_off_), 0);
    EXPECT_EQ(index->offset(0, index->doc_counts_off_), 0);
    EXPECT_EQ(index->offset("c", index->doc_counts_off_), 2);
    EXPECT_EQ(index->offset(1, index->doc_counts_off_), 2);
    EXPECT_EQ(index->offset("z", index->doc_counts_off_), 3);
    EXPECT_EQ(index->offset(2, index->doc_counts_off_), 3);
}

TEST_F(IndexLoading, posting_ranges)
{
    auto bystring = index->posting_range("b", FakeScore{});
    auto byid = index->posting_range(0, FakeScore{});
    std::vector<Posting> bystring_actual;
    std::vector<Posting> byid_actual;
    std::vector<Posting> expected = {{0, 1.0}, {1, 2.0}};
    for (const auto& posting : bystring) {
        bystring_actual.push_back(posting);
    }
    for (const auto& posting : byid) {
        byid_actual.push_back(posting);
    }
    EXPECT_THAT(bystring_actual, ::testing::ElementsAreArray(expected));
    EXPECT_THAT(byid_actual, ::testing::ElementsAreArray(expected));
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

