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
//! \author     Michal Siedlaczek
//! \copyright  MIT License

#define CATCH_CONFIG_MAIN

#include <functional>

#include <catch2/catch.hpp>

#include <irkit/algorithm/query.hpp>

struct UnscoredPosting {
    int doc;
    int freq;
    int document() const { return doc; }
    int payload() const { return freq; }
};

struct ScoredPosting {
    int doc;
    double score;
    int document() const { return doc; }
    double payload() const { return score; }
};

auto unscored_postings()
{
    return std::vector<std::vector<UnscoredPosting>>{
        {{3, 2}},
        {{0, 1}, {2, 3}, {6, 2}},
        {{2, 1}, {3, 1}, {6, 1}, {12, 4}}};
}

auto scorers()
{
    return std::vector<std::function<double(int, int)>>{
        [](int doc, int freq) { return doc + freq * 2; },
        [](int doc, int freq) { return doc + freq * 3; },
        [](int doc, int freq) { return doc + freq * 1.5; }};
}

auto scored_postings()
{
    return std::vector<std::vector<ScoredPosting>>{
        {{3, 7.0}},
        {{0, 3.0}, {2, 11.0}, {6, 12.0}},
        {{2, 3.5}, {3, 4.5}, {6, 7.5}, {12, 18.0}}};
}

using result_type = std::pair<int, double>;
using result_list = std::vector<result_type>;

auto expected_top_3()
{
    return result_list{std::make_pair(2, 14.5),
                       std::make_pair(6, 19.5),
                       std::make_pair(12, 18)};
}

int collection_size()
{
    return 20;
}

bool operator==(const result_type& lhs, const result_type& rhs)
{
    return lhs.first == rhs.first && lhs.second == rhs.second;
}

std::ostream& operator<<(std::ostream& os, const result_type& r)
{
    return os << "(" << r.first << ", " << r.second << ")";
}

class UnorderedVectorEq : public Catch::MatcherBase<result_list> {
    result_list list_;
public:
    UnorderedVectorEq(result_list list) : list_(std::move(list)) {}

    bool match(const result_list& other) const override
    {
        if (list_.size() != other.size()) {
            return false;
        }
        std::unordered_map<int, double> result_map;
        for (auto&& [doc, score] : list_) {
            if (auto pos = result_map.find(doc); pos != result_map.end()) {
                return false;
            }
            result_map[doc] = score;
        }
        for (auto&& [doc, score] : other) {
            if (auto pos = result_map.find(doc); pos != result_map.end()) {
                if (pos->second != score) {
                    return false;
                }
            } else {
                return false;
            }
        }
        return true;
    }

    std::string describe() const override {
        std::ostringstream ss;
        ss << "has the same elements as";
        for (const auto& elem : list_) {
            ss << " " << elem;
        }
        return ss.str();
    }
};

inline UnorderedVectorEq UnorderedEquals(result_list list)
{
    return UnorderedVectorEq(std::move(list));
}

TEST_CASE("TAAT", "[query_algorithm]")
{
    GIVEN("Scored posting lists")
    {
        const auto postings = scored_postings();
        WHEN("Processed with k = 3")
        {
            int k = 3;
            auto results =
                irk::taat(gsl::make_span(postings), collection_size(), k);
            THEN("Top k results are correct")
            {
                REQUIRE(results.size() == k);
                REQUIRE_THAT(results, UnorderedEquals(expected_top_3()));
            }
        }
    }
    GIVEN("Unscored posting lists")
    {
        const auto postings = unscored_postings();
        WHEN("Processed with k = 3")
        {
            int k = 3;
            auto results = irk::taat(
                gsl::make_span(postings),
                gsl::make_span(scorers()),
                collection_size(),
                k);
            THEN("Top k results are correct")
            {
                REQUIRE(results.size() == k);
                REQUIRE_THAT(results, UnorderedEquals(expected_top_3()));
            }
        }
    }
}

TEST_CASE("DAAT", "[query_algorithm]")
{
    GIVEN("Scored posting lists")
    {
        const auto postings = scored_postings();
        WHEN("Processed with k = 3")
        {
            int k = 3;
            auto results = irk::daat(gsl::make_span(postings), k);
            THEN("Top k results are correct")
            {
                REQUIRE(results.size() == k);
                REQUIRE_THAT(results, UnorderedEquals(expected_top_3()));
            }
        }
    }
    GIVEN("Unscored posting lists")
    {
        const auto postings = unscored_postings();
        WHEN("Processed with k = 3")
        {
            int k = 3;
            result_list results = irk::daat(
                gsl::make_span(postings), gsl::make_span(scorers()), k);
            THEN("Top k results are correct")
            {
                REQUIRE(results.size() == k);
                for (auto [doc, score] : results) {
                    std::cerr << " (" << doc << ", " << score << ")";
                }
                std::cerr << "DSFASF\n";
                REQUIRE_THAT(results, UnorderedEquals(expected_top_3()));
            }
        }
    }
}
