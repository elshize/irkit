#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <irkit/bitstream.hpp>
#include <irkit/coding/prefix_codec.hpp>
#include <irkit/prefixmap.hpp>

namespace {

namespace fs = boost::filesystem;

using map_type = irk::prefix_map<int, std::vector<char>>;
using block_builder = irk::prefix_map<int, std::vector<char>>::block_builder;
using block_ptr = irk::prefix_map<int, std::vector<char>>::block_ptr;

TEST(hutucker, individual_coding)
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
    irk::hutucker_codec<char> codec(frequencies);

    std::ifstream in_terms(terms_file);
    std::string term;
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

TEST(hutucker, prefix_coding)
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
    auto codec = irk::hutucker_codec<char>(frequencies);
    irk::prefix_codec<irk::hutucker_codec<char>> pref_codec(std::move(codec));

    int count = 0;
    std::vector<std::string> terms;
    std::ostringstream out_buffer;
    irk::output_bit_stream bout(out_buffer);
    std::string term, last = "";
    std::ifstream in_terms(terms_file);
    while (std::getline(in_terms, term)) {
        std::cout << term << std::endl;
        pref_codec.encode(term, bout);
        terms.push_back(term);
        ++count;
        break;
    }
    bout.flush();

    std::istringstream in_buffer(out_buffer.str());
    irk::input_bit_stream bin(in_buffer);
    for (int idx = 0; idx < count; ++idx) {
        std::string term;
        pref_codec.decode(bin, term);
        ASSERT_THAT(term, ::testing::ElementsAreArray(terms[idx]))
            << terms[idx] << "(" << idx << ")";
    }
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

