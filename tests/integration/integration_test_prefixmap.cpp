#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <irkit/prefixmap.hpp>

std::string terms_file;

namespace {

namespace fs = boost::filesystem;

using map_type = irk::prefix_map<int, std::vector<char>>;
using block_builder = irk::prefix_map<int, std::vector<char>>::block_builder;
using block_ptr = irk::prefix_map<int, std::vector<char>>::block_ptr;

TEST(prefix_map, build_load_verify)
{
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
        ASSERT_EQ(result.has_value(), true);
        std::cout << result.value() << " ; idx: " << idx << std::endl;
        ASSERT_EQ(result.value(), idx);
        idx++;
    }

    //std::ifstream in(in_file.c_str());
    //std::vector<std::string> strings;
    //std::string line;
    //while (std::getline(in, line)) {
    //    strings.push_back(line);
    //}
    //in.close();
    //std::sort(strings.begin(), strings.end());
    //auto map = irk::build_prefix_map<int>(strings, 128);
    //std::ostringstream out;
    //map.dump(out);

    //std::istringstream inl(out.str());
    //auto lmap = irk::load_prefix_map<int>(inl);

    //for (std::size_t idx = 0; idx < strings.size(); ++idx) {
    //    auto key = strings[idx];
    //    auto retrieved_idx = lmap[key];
    //    ASSERT_NE(retrieved_idx, std::nullopt) << key;
    //    ASSERT_EQ(retrieved_idx.value(), idx);
    //}
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    std::cout << argc << std::endl;
    std::cout << argv[0] << std::endl;
    //assert(argc == 2);
    // TODO: Figure out how to pass relative paths.
    //       This will only work if run from the folder of executable.
    terms_file = "terms.txt";
    return RUN_ALL_TESTS();
}

