#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public
#include <irkit/prefixmap.hpp>
#include <irkit/lexicon.hpp>

namespace {

namespace fs = boost::filesystem;

using map_type = irk::prefix_map<int, std::vector<char>>;
using block_builder = irk::prefix_map<int, std::vector<char>>::block_builder;
using block_ptr = irk::prefix_map<int, std::vector<char>>::block_ptr;

TEST(lexicon, build_load_verify)
{
    std::string terms_file("terms.txt");
    std::vector<std::string> lines;
    irk::io::load_lines(fs::path(terms_file), lines);

    // Build
    auto lexicon = irk::build_lexicon(lines, 64);

    // Dump
    std::vector<char> buffer;
    boost::iostreams::stream<
        boost::iostreams::back_insert_device<std::vector<char>>>
        out(boost::iostreams::back_inserter(buffer));
    lexicon.serialize(out);
    out.flush();

    // Load
    auto loaded_lex = irk::load_lexicon(irk::make_memory_view(buffer));
    std::vector<std::string> all_keys(loaded_lex.begin(), loaded_lex.end());

    ASSERT_THAT(lexicon.leading_indices_,
        ::testing::ElementsAreArray(loaded_lex.leading_indices_));
    ASSERT_THAT(lexicon.block_offsets_,
        ::testing::ElementsAreArray(loaded_lex.block_offsets_));
    ASSERT_EQ(lexicon.count_, loaded_lex.count_);
    ASSERT_EQ(lexicon.keys_per_block_, loaded_lex.keys_per_block_);
    std::vector<char> loaded_blocks(
        loaded_lex.blocks_.begin(), loaded_lex.blocks_.end());
    ASSERT_THAT(lexicon.blocks_, ::testing::ElementsAreArray(loaded_blocks));

    int idx = 0;
    auto it = loaded_lex.begin();
    std::cout << "TESTING\n";
    for (const auto& term : lines) {
        std::cout << term << std::endl;
        auto res = loaded_lex.index_at(term);
        ASSERT_EQ(*it, term);
        ASSERT_TRUE(res.has_value());
        ASSERT_EQ(res.value(), idx);
        auto t = loaded_lex.key_at(idx);
        ASSERT_EQ(t, term);
        ++it;
        ++idx;
    }
}

TEST(prefix_map, build_load_verify)
{
    std::string terms_file("terms.txt");

    // Build
    //auto map = irk::build_prefix_map_from_file<int>(terms_file);

    //// Dump
    //std::ostringstream out;
    //map.dump(out);

    //// Load
    //std::istringstream in(out.str());
    //auto loaded_map = irk::load_prefix_map<int>(in);

    //std::string x = u8"年美丽大变身计划";
    //std::ifstream in_terms(terms_file);
    //std::string term;
    //int idx = 0;
    //auto iter = map.begin();
    //while (std::getline(in_terms, term)) {
    //    //auto result = loaded_map[term];
    //    //ASSERT_EQ(result.has_value(), true) << term;
    //    //ASSERT_EQ(result.value(), idx);
    //    std::string t = *iter;
    //    //if (t != term) std::cout << loaded_map.codec_->encode(x) << std::endl;
    //    ASSERT_EQ(t, term) << term;
    //    idx++;
    //    ++iter;
    //}
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

