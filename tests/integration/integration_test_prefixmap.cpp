#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <irkit/prefixmap.hpp>

namespace {

namespace fs = boost::filesystem;

using map_type = irk::prefix_map<int, std::vector<char>>;
using block_builder = irk::prefix_map<int, std::vector<char>>::block_builder;
using block_ptr = irk::prefix_map<int, std::vector<char>>::block_ptr;

TEST(prefix_map, build_load_verify)
{
    std::string terms_file("collection.txt");

    // Build
    auto map = irk::build_prefix_map_from_file<int>(terms_file);

    // Dump
    std::ostringstream out;
    map.dump(out);

    // Load
    std::istringstream in(out.str());
    auto loaded_map = irk::load_prefix_map<int>(in);

    std::ifstream in_terms(terms_file);
    std::string term;
    int idx = 0;
    while (std::getline(in_terms, term)) {
        auto result = loaded_map[term];
        ASSERT_EQ(result.has_value(), true) << term;
        ASSERT_EQ(result.value(), idx);
        idx++;
    }
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

