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

    // Load
    auto loaded_lex = irk::load_lexicon(irk::make_memory_view(buffer));

    int idx = 0;
    for (const auto& term : lines) {
        auto res = lexicon.index_at(term);
        ASSERT_TRUE(res.has_value());
        ASSERT_EQ(res.value(), idx++);
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

