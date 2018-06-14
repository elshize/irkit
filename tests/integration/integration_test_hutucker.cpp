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

TEST(hutucker, build_verify)
{
    std::string terms_file("terms.txt");

    // Build
    std::ifstream in(terms_file.c_str());
    std::vector<std::size_t> frequencies(256, 0);
    std::string item;
    while (std::getline(in, item)) {
        for (const char& ch : item) {
            ++frequencies[static_cast<unsigned char>(ch)];
        }
    }
    in.close();
    irk::coding::hutucker_codec<char> codec(frequencies);

    std::ifstream in_terms(terms_file);
    std::string term;
    //int idx = 0;
    while (std::getline(in_terms, term)) {
        std::ostringstream enc;
        auto encoded = codec.encode(term.begin(), term.end());
        std::vector<char> data(encoded.size() / 8 + 1, 0);
        irk::bitptr<char> bp(data.data());
        irk::bitcpy(bp, encoded);
        auto reader = bp.reader();
        codec.decode(reader, enc, term.size());
        ASSERT_THAT(term, ::testing::ElementsAreArray(enc.str())) << term;
    }
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

