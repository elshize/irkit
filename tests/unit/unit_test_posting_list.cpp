// MIT License
//
// Copyright (c) 2018 Michal Siedlaczek
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

//! \file
//! \author Michal Siedlaczek
//! \copyright MIT License

#include <algorithm>
#include <sstream>
#include <vector>

#include <cppitertools/itertools.hpp>
#include <gmock/gmock.h>
#include <gsl/span>
#include <gtest/gtest.h>

#define private public
#define protected public
#include <irkit/index/vector_inverted_list.hpp>
#include <irkit/index/posting_list.hpp>
#include <irkit/movingrange.hpp>

namespace {

namespace tmp {

    template<typename T>
    using OrderFn = std::function<bool(const T&, const T&)>;

    template<typename Iterator>
    using range_vector =
        std::vector<std::pair<std::ptrdiff_t, irk::moving_range<Iterator>>>;

    template<typename T, typename Iterator>
    class PositionedUnionIterator
        : public boost::iterator_facade<
              PositionedUnionIterator<T, Iterator>,
              std::pair<std::ptrdiff_t, T>,
              boost::forward_traversal_tag,
              std::pair<std::ptrdiff_t, const T&>> {
    public:
        using OrderFn = irk::OrderFn<irk::moving_range<Iterator>>;
        PositionedUnionIterator(
            range_vector<Iterator> ranges,
            OrderFn order,
            size_t pos,
            size_t length)
            : ranges_(std::move(ranges)),
              order_(std::move(order)),
              pos_(pos),
              length_(length)
        {
            std::sort(
                ranges_.begin(),
                ranges_.end(),
                [this](const auto& lhs, const auto& rhs) {
                    return order_(lhs.second, rhs.second);
                });
        }

    private:
        friend class boost::iterator_core_access;
        void increment()
        {
            if (pos_ == length_) {
                return;
            }
            ranges_.front().second.advance();
            for (auto it = std::next(ranges_.begin()); it != ranges_.end();
                 ++it) {
                auto prev = std::prev(it);
                if (order_(prev->second, it->second)) {
                    break;
                }
                std::iter_swap(prev, it);
            }
            pos_ += 1;
        }
        bool equal(const PositionedUnionIterator& other) const
        {
            return pos_ == other.pos_;
        }
        std::pair<std::ptrdiff_t, const T&> dereference() const
        {
            return std::make_pair(
                ranges_.front().first, ranges_.front().second.front());
        }

        range_vector<Iterator> ranges_;
        OrderFn order_;
        size_t pos_;
        size_t length_;
    };

    template<typename T, typename PostingList>
    class PositionedUnion {
        using list_iterator = typename PostingList::iterator;

        range_vector<list_iterator> retrieve_ranges(
            std::function<irk::moving_range<list_iterator>(const PostingList&)>
                fun) const
        {
            range_vector<list_iterator> ranges;
            std::ptrdiff_t id{0};
            std::transform(
                lists_.begin(),
                lists_.end(),
                std::back_inserter(ranges),
                [&fun, &id](const auto& list) {
                    return std::make_pair(id++, fun(list));
                });
            return ranges;
        }

    public:
        PositionedUnion(
            std::vector<PostingList> lists,
            irk::OrderFn<irk::moving_range<list_iterator>> order)
            : lists_(std::move(lists)), order_(std::move(order))
        {
            length_ = std::accumulate(
                lists_.begin(),
                lists_.end(),
                0,
                [](const auto acc, const auto& list) {
                    return acc + list.size();
                });
        }
        auto begin() const
        {
            return PositionedUnionIterator<T, list_iterator>(
                retrieve_ranges([](const auto& list) {
                    return irk::moving_range(list.begin(), list.end());
                }),
                order_,
                0,
                length_);
        }
        auto end() const
        {
            return PositionedUnionIterator<T, list_iterator>(
                retrieve_ranges([](const auto& list) {
                    return irk::moving_range(list.end(), list.end());
                }),
                order_,
                length_,
                length_);
        }
        auto size() const { return length_; }

    private:
        std::vector<PostingList> lists_;
        irk::OrderFn<irk::moving_range<list_iterator>> order_;
        size_t length_;
    };

    template<typename Iter>
    auto merge_positioned(Iter first_posting_list, Iter last_posting_list)
    {
        using list_type = std::remove_const_t<
            std::remove_reference_t<decltype(*first_posting_list)>>;
        using list_iterator = typename list_type::iterator;
        return PositionedUnion<typename list_type::posting_view, list_type>(
            std::vector<list_type>(first_posting_list, last_posting_list),
            [](const irk::moving_range<list_iterator>& lhs,
               const irk::moving_range<list_iterator>& rhs) {
                if (rhs.empty()) {
                    return true;
                }
                if (lhs.empty()) {
                    return false;
                }
                return lhs.front().document() < rhs.front().document();
            });
    }

    template<typename Range>
    auto merge_positioned(const Range& posting_lists)
    {
        return merge_positioned(posting_lists.begin(), posting_lists.end());
    }

}  // namespace tmp

TEST(posting_list_view, forward_iterator)
{
    std::vector<long> documents = {0, 1, 4, 6, 9, 11, 30};
    std::vector<double> payloads = {0, 1, 4, 6, 9, 11, 30};
    irk::vector_document_list vdl(0, documents);
    irk::vector_payload_list vpl(0, payloads);
    vdl.block_size(3);
    vpl.block_size(3);

    irk::posting_list_view postings(vdl, vpl);
    int idx = 0;
    for (auto posting : postings) {
        ASSERT_EQ(posting.document(), documents[idx]);
        ASSERT_EQ(posting.payload(), payloads[idx]);
        idx++;
    }
}

template<class T>
std::pair<long, double> pair_of(const T& posting)
{
    long doc = posting->document();
    double pay = posting->payload();
    return std::make_pair(doc, pay);
}

TEST(posting_list_view, lookup)
{
    std::vector<long> documents = {0, 1, 4, 6, 9, 11, 30};
    std::vector<double> payloads = {0, 1, 4, 6, 9, 11, 30.1};
    irk::vector_document_list vdl(0, documents);
    irk::vector_payload_list vpl(0, payloads);
    vdl.block_size(3);
    vpl.block_size(3);

    irk::posting_list_view postings(vdl, vpl);

    std::vector<std::pair<long, double>> p;
    for (auto it = postings.begin(); it != postings.end(); ++it) {
        p.emplace_back(it->document(), it->payload());
    }
    std::vector<std::pair<long, double>> e = {{0, 0}, {1, 1}, {4, 4}, {6, 6}, {9, 9}, {11, 11}, {30, 30.1}};
    ASSERT_THAT(p, ::testing::ElementsAreArray(e));

    ASSERT_THAT(pair_of(postings.lookup(1)), ::testing::Pair(1, 1));
    ASSERT_THAT(pair_of(postings.lookup(2)), ::testing::Pair(4, 4));
    ASSERT_THAT(pair_of(postings.lookup(3)), ::testing::Pair(4, 4));
    ASSERT_THAT(pair_of(postings.lookup(4)), ::testing::Pair(4, 4));
    ASSERT_THAT(pair_of(postings.lookup(5)), ::testing::Pair(6, 6));
    ASSERT_THAT(pair_of(postings.lookup(6)), ::testing::Pair(6, 6));
    ASSERT_THAT(pair_of(postings.lookup(7)), ::testing::Pair(9, 9));
    ASSERT_THAT(pair_of(postings.lookup(8)), ::testing::Pair(9, 9));
    ASSERT_THAT(pair_of(postings.lookup(9)), ::testing::Pair(9, 9));
    ASSERT_THAT(pair_of(postings.lookup(15)), ::testing::Pair(30, 30.1));
    ASSERT_THAT(pair_of(postings.lookup(30)), ::testing::Pair(30, 30.1));
    ASSERT_EQ(postings.lookup(31), postings.end());
}

TEST(posting_list_view, union_view)
{
    std::vector<irk::posting_list_view<
        irk::vector_document_list<int>,
        irk::vector_payload_list<int>>>
        posting_lists;
    posting_lists.emplace_back(
        irk::vector_document_list(0, std::vector<int>{0, 1, 4}),
        irk::vector_payload_list(0, std::vector<int>{0, 0, 0}));
    posting_lists.emplace_back(
        irk::vector_document_list(0, std::vector<int>{0, 2, 4}),
        irk::vector_payload_list(0, std::vector<int>{1, 1, 1}));
    posting_lists.emplace_back(
        irk::vector_document_list(0, std::vector<int>{1, 2, 4}),
        irk::vector_payload_list(0, std::vector<int>{2, 2, 2}));

    auto postings = irk::merge(posting_lists);
    std::vector<int> docs_only(postings.size());
    std::transform(
        postings.begin(),
        postings.end(),
        docs_only.begin(),
        [](const auto& posting) { return posting.document(); });
    ASSERT_THAT(docs_only, ::testing::ElementsAre(0, 0, 1, 1, 2, 2, 4, 4, 4));
}

//TEST(posting_list_view, pair_conversion)
//{
//    std::vector<long> documents = {0, 1, 4, 6, 9, 11, 30};
//    std::vector<double> payloads = {0, 1, 4, 6, 9, 11, 30.1};
//    irk::vector_document_list vdl(documents);
//    irk::vector_payload_list vpl(payloads);
//    vdl.block_size(3);
//    vpl.block_size(3);
//
//    irk::posting_list_view postings(vdl, vpl);
//    std::vector<std::pair<long, double>> pairs(
//        postings.begin(), postings.end());
//    std::vector<std::pair<long, double>> expected = {
//        {0, 0}, {1, 1}, {4, 4}, {6, 6}, {9, 9}, {11, 11}, {30, 30.1}};
//    ASSERT_THAT(pairs, ::testing::ElementsAreArray(expected));
//}

TEST(scored_posting_list_view, forward_iterator)
{
    std::vector<long> documents = {0, 1, 4, 6, 9, 11, 30};
    std::vector<double> payloads = {0, 1, 4, 6, 9, 11, 30};
    irk::vector_document_list vdl(0, documents);
    irk::vector_payload_list vpl(0, payloads);
    vdl.block_size(3);
    vpl.block_size(3);

    auto plus_one = [](auto doc, auto tf){
        return tf + 1;
    };

    irk::posting_list_view postings(vdl, vpl);
    int idx = 0;
    auto scored_postings = postings.scored(plus_one);
    for (auto posting : scored_postings) {
        ASSERT_EQ(posting.document(), documents[idx]);
        ASSERT_EQ(posting.payload(), payloads[idx] + 1);
        idx++;
    }
}

TEST(positioned_merge, iterator)
{
    std::vector<std::pair<std::vector<int>, std::vector<double>>> lists{
        {{0, 1, 4, 6, 9, 11, 30}, {0, 1, 4, 6, 9, 11, 30}},
        {{0, 1, 6, 9, 20, 30}, {0, 1, 6, 9, 20, 30}}};
    std::vector<irk::posting_list_view<std::vector<int>, std::vector<double>>>
        postings = {{lists[0].first, lists[0].second},
                    {lists[1].first, lists[1].second}};
    std::vector<int> expected_docs = {0, 0, 1, 1, 4, 6, 6, 9, 9, 11, 20, 30, 30};
    std::vector<std::pair<std::ptrdiff_t, double>> expected_pairs = {
        {0, 0}, {1, 0}, {0, 1}, {1, 1}, {0, 4}, {0, 6}, {1, 6}, {0, 9}, {1, 9},
        {0, 11}, {1, 20}, {0, 30}, {1, 30}};

    auto merged = tmp::merge_positioned(postings);

    std::vector<int> docs;
    std::vector<std::pair<std::ptrdiff_t, double>> pairs;
    auto iter = merged.begin();
    for (auto [list_id, posting] : merged) {
        docs.push_back(posting.document());
        pairs.push_back(std::make_pair(list_id, posting.payload()));
    }
    ASSERT_THAT(docs, ::testing::ElementsAreArray(expected_docs));
}

}  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
