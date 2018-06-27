#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <irkit/radix_tree.hpp>
#include <irkit/io.hpp>

namespace {

namespace fs = boost::filesystem;

TEST(radix_tree, build_read)
{
    int block_size = 16;
    std::string terms_file("terms.txt");
    std::vector<std::string> lines;
    irk::io::load_lines(fs::path(terms_file), lines);

    // Build
    irk::radix_tree<int> t;
    int idx = 0;
    std::ifstream in_terms(terms_file);
    for (const std::string& term : lines)
    {
        if (idx % block_size == 0) { t.insert(term, idx / block_size); }
        idx++;
    }
    //std::cout << idx << std::endl;

    idx = 0;
    in_terms.clear();
    in_terms.seekg(0, std::ios::beg);
    for (const std::string& term : lines)
    {
        auto exact = t.find(term);
        if (idx % block_size == 0) {
            ASSERT_TRUE(exact.has_value());
            ASSERT_EQ(exact.value(), idx / block_size);
        } else {
            ASSERT_FALSE(exact.has_value());
        }
        auto leq = t.seek_le(term);
        ASSERT_EQ(leq.value(), idx / block_size);
        idx++;
    }
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

