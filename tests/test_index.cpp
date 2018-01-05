#include <experimental/filesystem>
#include <iostream>
#define private public
#define protected public
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "index.hpp"
#include "irkit/daat.hpp"
#include "irkit/taat.hpp"
#include "query.hpp"
#include "saat.hpp"

namespace {

using namespace bloodhound;

class IndexTest : public ::testing::Test {
  protected:
	index::Index<index::InMemoryPostingPolicy> index;
	std::vector<Score> default_weights;
	std::vector<PostingList> default_postings;

	virtual void SetUp() {
		index.collection_size = 3;
		std::vector<uint32_t> postings_ints = {
			0, 3, 0, 36, 0, 0, // term 0 header
			0, 1, 2, 3, 3, 3, // term 0 postings
			0, 2, 0, 32, 0, 0, // term 1 header
			0, 2, 3, 3, // term 1 postings
			1 << 28, 1, 0, 3, 0, 0, // term 2 header
		};
		char* posting_mem = reinterpret_cast<char*>(postings_ints.data());
		std::size_t posting_mem_size =
			postings_ints.size() * sizeof(postings_ints[0]);
		index.postings_data.insert(
				index.postings_data.begin(),
				posting_mem,
				posting_mem + posting_mem_size);

		index.lexicon[TermId(0)] = Offset(0);
		index.lexicon[TermId(1)] = Offset(48);
		index.lexicon[TermId(2)] = Offset(88);
		default_weights = {Score(1), Score(1)};
		default_postings = {
			index.posting_list(TermId(0)),
			index.posting_list(TermId(2))};
    }
};

class WandTest : public IndexTest {
protected:
    virtual void SetUp()
    {
        IndexTest::SetUp();
        index.max_scores = {{TermId(0), Score(3)},
            {TermId(1), Score(3)},
            {TermId(2), Score(3)}};
        default_postings = {
            index.posting_list(TermId(0)), index.posting_list(TermId(2))};
    }
};

class PivotTest : public WandTest {
protected:
    irkit::Heap<Doc, unsigned int> list_heap;

    virtual void SetUp()
    {
        WandTest::SetUp();
        for (std::size_t idx = 0; idx < default_postings.size(); ++idx) {
            list_heap.push(default_postings[idx].docs[0], idx);
        }
    }
};

class SaatTest : public IndexTest {
};

//TEST_F(IndexTest, posting_range)
//{
//    auto postlist = index.posting_list(TermId(0));
//    auto range = postlist.posting_range();
//    std::vector<Posting> expected = {
//        {Doc(0), Score(3)}, {Doc(1), Score(3)}, {Doc(2), Score(3)}};
//    std::vector<Posting> actual(range);
//    EXPECT_THAT(actual, ::testing::ElementsAreArray(expected));
//}

TEST_F(IndexTest, posting_list_next_ge)
{
    auto post_list = index.posting_list(TermId(0));
    auto current = post_list.next_ge(post_list.begin(), Doc(1));
    ASSERT_EQ(Doc(1), current->doc);

    post_list = index.posting_list(TermId(1));
    current = post_list.next_ge(post_list.begin(), Doc(1));
    ASSERT_EQ(Doc(2), current->doc);

    current = post_list.next_ge(current, Doc(3));
    ASSERT_EQ(current, post_list.end());
}

TEST_F(IndexTest, posting_list_0)
{
	std::vector<Doc> expected_docs = {Doc(0), Doc(1), Doc(2)};
	std::vector<Score> expected_scores = {Score(3), Score(3), Score(3)};
	auto post_list = index.posting_list(TermId(0));
	EXPECT_EQ(3, post_list.docs.size());
	EXPECT_EQ(3, post_list.scores.size());
	EXPECT_THAT(expected_docs, ::testing::ElementsAreArray(post_list.docs));
	EXPECT_THAT(expected_scores, ::testing::ElementsAreArray(post_list.scores));
}

TEST_F(IndexTest, posting_list_1)
{
	std::vector<Doc> expected_docs = {Doc(0), Doc(2)};
	std::vector<Score> expected_scores = {Score(3), Score(3)};
	auto post_list = index.posting_list(TermId(1));
	EXPECT_EQ(2, post_list.docs.size());
	EXPECT_EQ(2, post_list.scores.size());
	EXPECT_THAT(expected_docs, ::testing::ElementsAreArray(post_list.docs));
	EXPECT_THAT(expected_scores, ::testing::ElementsAreArray(post_list.scores));
}

TEST_F(IndexTest, posting_list_2)
{
	std::vector<Doc> expected_docs = {Doc(1)};
	std::vector<Score> expected_scores = {Score(3)};
	auto post_list = index.posting_list(TermId(2));
	EXPECT_EQ(1, post_list.docs.size());
	EXPECT_EQ(1, post_list.scores.size());
	EXPECT_THAT(expected_docs, ::testing::ElementsAreArray(post_list.docs));
	EXPECT_THAT(expected_scores, ::testing::ElementsAreArray(post_list.scores));
}

TEST_F(IndexTest, posting_list_nonexistent)
{
	std::vector<Doc> expected_docs = {Doc(1)};
	std::vector<Score> expected_scores = {Score(3)};
	auto post_list = index.posting_list(TermId(3));
	EXPECT_EQ(0, post_list.docs.size());
	EXPECT_EQ(0, post_list.scores.size());
}

TEST_F(IndexTest, taat)
{
	query::TaatRetriever<PostingList> retriever(index.get_collection_size());
	retriever.traverse(default_postings, default_weights);
	std::vector<Score> expected_acc = {Score(3), Score(6), Score(3)};
	EXPECT_THAT(expected_acc, ::testing::ElementsAreArray(retriever.accumulator_array));

	auto results = retriever.aggregate_top(2);
	std::vector<query::Result> expected_results = {
		{Doc(1), Score(6)},
		{Doc(2), Score(3)}
	};
	EXPECT_THAT(expected_results, ::testing::ElementsAreArray(results));
}

//TEST_F(IndexTest, taat_init_gap)
//{
//	query::TaatRetriever<PostingList, false, 4> retriever(index.get_collection_size());
//
//	retriever.traverse(default_postings, default_weights);
//	std::vector<Score> expected_acc = {Score(3), Score(6), Score(3)};
//	EXPECT_THAT(expected_acc, ::testing::ElementsAreArray(retriever.accumulator_array));
//
//	auto results = retriever.aggregate_top(2);
//	std::vector<query::Result> expected_results = {
//		{Doc(1), Score(6)},
//		{Doc(2), Score(3)}
//	};
//	EXPECT_THAT(results, ::testing::ElementsAreArray(expected_results));
//
//	// The same query on the same accumulator: will not erase
//	retriever.traverse(default_postings, default_weights);
//	expected_acc = {Score(6), Score(12), Score(6)};
//	EXPECT_THAT(expected_acc, ::testing::ElementsAreArray(retriever.accumulator_array));
//
//	// The next query on the same accumulator: will erase
//	retriever.next_query();
//	auto bit_shift = sizeof(Score) * 8 - 2;
//	retriever.traverse(default_postings, default_weights);
//	expected_acc = {Score(3 | (1 << bit_shift)), Score(6 | (1 << bit_shift)), Score(3 | (1 << bit_shift))};
//	EXPECT_THAT(expected_acc, ::testing::ElementsAreArray(retriever.accumulator_array));
//}
//
//TEST_F(IndexTest, taat_init_gap_and_acc_blocks)
//{
//	query::TaatRetriever<PostingList, false, 4, 2> retriever(index.get_collection_size());
//
//	retriever.traverse(default_postings, default_weights);
//	std::vector<Score> expected_acc = {Score(3), Score(6), Score(3)};
//	EXPECT_THAT(expected_acc, ::testing::ElementsAreArray(retriever.accumulator_array));
//
//	auto results = retriever.aggregate_top(2);
//	std::vector<query::Result> expected_results = {
//		{Doc(1), Score(6)},
//		{Doc(2), Score(3)}
//	};
//	EXPECT_THAT(expected_results, ::testing::ElementsAreArray(results));
//
//	// The same query on the same accumulator: will not erase
//	retriever.traverse(default_postings, default_weights);
//	expected_acc = {Score(6), Score(12), Score(6)};
//	EXPECT_THAT(expected_acc, ::testing::ElementsAreArray(retriever.accumulator_array));
//
//	// The next query on the same accumulator: will erase
//	retriever.next_query();
//	auto bit_shift = sizeof(Score) * 8 - 2;
//	retriever.traverse(default_postings, default_weights);
//	expected_acc = {Score(3 | (1 << bit_shift)), Score(6 | (1 << bit_shift)), Score(3 | (1 << bit_shift))};
//	EXPECT_THAT(expected_acc, ::testing::ElementsAreArray(retriever.accumulator_array));
//}

TEST_F(IndexTest, daat)
{
	query::DaatRetriever<PostingList> daat;
	auto results = daat.retrieve(default_postings, default_weights, 2);
	std::vector<query::Result> expected_results = {
		{Doc(1), Score(6)},
		{Doc(2), Score(3)}
	};
	EXPECT_THAT(results, ::testing::ElementsAreArray(expected_results));
}

//TEST_F(PivotTest, zero_threshold)
//{
//	query::WandRetriever<PostingList> wand;
//	Score threshold = Score(0);
//	auto buf = wand.select_pivot(
//			default_postings, list_heap, default_weights, threshold);
//	std::vector<irkit::Entry<Doc, unsigned int>> expected_buf = {
//		{Doc(0), 0u}
//	};
//	EXPECT_EQ(expected_buf, buf);
//}
//
//TEST_F(PivotTest, threshold_equal_to_first_list)
//{
//	query::WandRetriever<PostingList> wand;
//	Score threshold = Score(3);
//	auto buf = wand.select_pivot(
//			default_postings, list_heap, default_weights, threshold);
//	std::vector<irkit::Entry<Doc, unsigned int>> expected_buf = {
//		{Doc(0), 0u}
//	};
//	EXPECT_EQ(expected_buf, buf);
//}
//
//TEST_F(PivotTest, threshold_extend_to_pivot_id)
//{
//	query::WandRetriever<PostingList> wand;
//	Score threshold = Score(3);
//
//	// Move the first posting list to the next document
//	list_heap.pop();
//	list_heap.push(default_postings[0].docs[1], 0);
//
//	auto buf = wand.select_pivot(
//			default_postings, list_heap, default_weights, threshold);
//	std::vector<irkit::Entry<Doc, unsigned int>> expected_buf = {
//		{Doc(1), 1u},
//		{Doc(1), 0u}  // expanded to pivot DocID
//	};
//	EXPECT_EQ(expected_buf, buf);
//}
//
//TEST_F(WandTest, wand)
//{
//	query::WandRetriever<PostingList> wand;
//	auto results = wand.retrieve(default_postings, default_weights, 2);
//	std::vector<query::Result> expected_results = {
//		{Doc(1), Score(6)},
//		{Doc(0), Score(3)}
//	};
//	EXPECT_THAT(results, ::testing::ElementsAreArray(expected_results));
//}

TEST_F(SaatTest, all_postings)
{
    query::ExactSaatRetriever<PostingList> retriever(3, 1.0);
	auto results = retriever.retrieve(default_postings, default_weights, 2);
	std::vector<query::Result> expected_results = {
		{Doc(1), Score(6)},
		{Doc(0), Score(3)}
	};
	EXPECT_THAT(results, ::testing::ElementsAreArray(expected_results));
}

TEST_F(SaatTest, two_postings)
{
    query::ExactSaatRetriever<PostingList> retriever(3, 0.5);
	auto results = retriever.retrieve(default_postings, default_weights, 2);
    EXPECT_EQ(retriever.get_posting_count(), 4);
    EXPECT_EQ(retriever.get_posting_threshold(), 2);
    EXPECT_EQ(retriever.get_processed_postings(), 2);
	std::vector<query::Result> expected_results = {
		{Doc(1), Score(3)},
		{Doc(0), Score(3)}
	};
	EXPECT_THAT(results, ::testing::ElementsAreArray(expected_results));
}

TEST(Taat, nbits)
{
	EXPECT_EQ(irkit::nbits(0), 0);
	EXPECT_EQ(irkit::nbits(1), 0);
	EXPECT_EQ(irkit::nbits(2), 1);
	EXPECT_EQ(irkit::nbits(4), 2);
	EXPECT_EQ(irkit::nbits(8), 3);
}

std::vector<TermWeight> parse_query(
    std::string& query_line)
{
    std::vector<TermWeight> query;
    std::string term_pair;
    std::istringstream line_stream(query_line);
    while (line_stream >> term_pair) {
        std::size_t pos = term_pair.find(":");
        term_pair[pos] = ' ';
        std::istringstream pair_stream(term_pair);
        TermId termid;
        Score score;
        pair_stream >> termid;
        pair_stream >> score;
        query.push_back({termid, score});
    }
    return query;
}

std::size_t count_duplicates(std::vector<query::Result>& results)
{
    std::unordered_map<Doc, std::size_t> counter;
    std::size_t duplicates = 0;
    for (auto& result : results) {
        counter[result.doc]++;
        if (counter[result.doc] == 2) duplicates++;
    }
    return duplicates;
}

TEST(Comparison, compare_retrievers)
{
    fs::path ukb("ukb_queries");
    std::string line;
    std::ifstream ukb_stream(ukb);
    std::vector<std::string> input_lines;
    while (std::getline(ukb_stream, line)) {
        input_lines.push_back(line);
    }

    std::vector<std::vector<TermWeight>> input;
    for (auto& line : input_lines) {
        std::vector<TermWeight> term_weights = parse_query(line);
        input.push_back(term_weights);
    }
    index::Index index = index::build_index_from_ids(input);
    index::Index sorted_index = index::sorted_index(index);

    std::size_t k = 10;

    query::TaatRetriever<PostingList, false, 0, 0> taat(
        index.get_collection_size());
    query::RawTaatRetriever<PostingList> raw_taat(index.get_collection_size());
    query::DaatRetriever<PostingList> daat;
    query::WandRetriever<PostingList> wand;
    query::MaxScoreRetriever<PostingList> mscore;
    query::TaatMaxScoreRetriever<PostingList> tmscore(index.get_collection_size());
    query::ExactSaatRetriever<PostingList> saat(index.get_collection_size());
    ASSERT_EQ(saat.et_threshold, 1.0);

    for (auto& line : input_lines) {
        std::vector<TermId> query;
        std::vector<Score> weights;
        auto term_weights = parse_query(line);
        for (auto& [term, weight] : term_weights) {
            query.push_back(term);
            weights.push_back(weight);
        }

        auto postings = index.terms_to_postings(query);
        auto taat_results = taat.retrieve(postings, weights, k);
        auto raw_taat_results = raw_taat.retrieve(postings, weights, k);
        auto daat_results =
            daat.retrieve(index.terms_to_postings(query), weights, k);
        auto wand_results =
            wand.retrieve(index.terms_to_postings(query), weights, k);
        auto mscore_results =
            mscore.retrieve(index.terms_to_postings(query), weights, k);
        auto tmscore_results =
            tmscore.retrieve(index.terms_to_postings(query), weights, k);
        auto saat_results =
            saat.retrieve(sorted_index.terms_to_postings(query), weights, k);

        ASSERT_EQ(saat.get_posting_count(), saat.get_posting_threshold());
        ASSERT_EQ(saat.get_posting_count(), saat.get_processed_postings());

        auto taat_duplicates = count_duplicates(taat_results);
        ASSERT_EQ(taat_duplicates, 0);

        auto daat_duplicates = count_duplicates(daat_results);
        ASSERT_EQ(daat_duplicates, 0);

        auto wand_duplicates = count_duplicates(wand_results);
        ASSERT_EQ(wand_duplicates, 0);

        auto saat_duplicates = count_duplicates(saat_results);
        ASSERT_EQ(saat_duplicates, 0);

        ASSERT_EQ(taat_results.size(), k);
        ASSERT_THAT(daat_results, ::testing::ElementsAreArray(taat_results));
        ASSERT_THAT(wand_results, ::testing::ElementsAreArray(taat_results));
        ASSERT_THAT(mscore_results, ::testing::ElementsAreArray(taat_results));
        ASSERT_THAT(tmscore_results, ::testing::ElementsAreArray(taat_results));
        ASSERT_THAT(saat_results, ::testing::ElementsAreArray(taat_results));
        ASSERT_THAT(
            raw_taat_results, ::testing::ElementsAreArray(taat_results));
        break;
    }

}

}  // namespace

int main(int argc, char ** argv)
{
	::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
