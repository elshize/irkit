#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <irkit/index.hpp>
#include <irkit/index/assembler.hpp>

namespace {

namespace fs = boost::filesystem;

auto postings_on_fly(fs::path collection_file)
{
    std::map<std::string, std::vector<std::pair<long, long>>> postings;
    std::map<std::string, std::map<long, long>> postings_map;
    std::ifstream input(collection_file);
    std::string line;
    long doc = 0;
    while (std::getline(input, line)) {
        std::istringstream line_input(line);
        std::string title, term;
        line_input >> title;
        while (line_input >> term) {
            std::cout << "t\n";
            postings_map[term][doc]++;
        }
        doc++;
    }
    for (const auto& [term, map] : postings_map) {
        for (const auto& [id, count] : map)
        { postings[term].push_back({id, count}); }
        std::sort(postings[term].begin(), postings[term].end());
        std::cout << term << " " << postings[term].size() << std::endl;
    }
    return postings;
}

TEST(inverted_index, build_load_verify)
{
    auto index_dir = irk::fs::temp_directory_path()
        / "InvertedIndexIntegrationTest";
    if (irk::fs::exists(index_dir)) {
        irk::fs::remove_all(index_dir);
    }
    irk::fs::create_directory(index_dir);

    // given
    std::string collection_file("collection.txt");
    auto expected_index = postings_on_fly(collection_file);
    std::ifstream input(collection_file);

    // when
    irk::index::index_assembler<long, std::string, long, long> assembler(
        fs::path(index_dir),
        32,
        1024,
        irk::coding::varbyte_codec<long>{},
        irk::coding::varbyte_codec<long>{});

    assembler.assemble(input);

    // then
    auto data = std::make_shared<irk::v2::inverted_index_mapped_data_source>(
        index_dir);
    irk::v2::inverted_index_view index_view(data.get(),
        irk::coding::varbyte_codec<long>{},
        irk::coding::varbyte_codec<long>{});

    for (long term_id = 0; term_id < index_view.terms().size(); term_id++) {
        auto postings = index_view.postings(term_id);
        const auto& expected = expected_index[index_view.term(term_id)];
        std::cout << index_view.term_id(index_view.term(term_id)).value_or(-1) << std::endl;
        std::vector<std::pair<long, long>> actual(
            postings.begin(), postings.end());
        ASSERT_THAT(actual, ::testing::ElementsAreArray(expected));
    }
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

