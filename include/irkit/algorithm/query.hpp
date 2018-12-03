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

#pragma once

#include <vector>
#include <iostream>

#include <boost/algorithm/string.hpp>
#include <gsl/span>

#include <irkit/algorithm/transform.hpp>
#include <irkit/io.hpp>
#include <irkit/movingrange.hpp>
#include <irkit/taat.hpp>
#include <irkit/utils.hpp>

namespace irk {

namespace detail {

    template<class T>
    // requires ScoredPostingList<T>
    using score_type =
        std::decay_t<decltype(std::declval<T>().begin()->payload())>;

    template<class T>
    // requires PostingList<T>
    using document_type =
        std::decay_t<decltype(std::declval<T>().begin()->document())>;

    auto moving_range_order = [](const auto& lhs, const auto& rhs) {
        if (rhs.empty()) {
            return true;
        }
        if (lhs.empty()) {
            return false;
        }
        return lhs.front().document() < rhs.front().document();
    };

    auto pair_moving_range_order = [](const auto& lhs, const auto& rhs) {
        if (rhs.second.empty()) {
            return true;
        }
        if (lhs.second.empty()) {
            return false;
        }
        return lhs.second.front().document() < rhs.second.front().document();
    };

    template<
        class T,
        class Iterator,
        class Range,
        class Document,
        class Score,
        class ExtractFn,
        class ScoreFn,
        class OrderFn>
    auto daat(
        gsl::span<const T> postings,
        int k,
        std::vector<Range> ranges,
        OrderFn order,
        ExtractFn extract_range,
        ScoreFn score_fn)
    {
        auto min_pos = std::min_element(ranges.begin(), ranges.end(), order);
        auto current_doc = extract_range(min_pos)->begin()->document();
        std::iter_swap(ranges.begin(), min_pos);

        irk::top_k_accumulator<Document, Score> acc(k);
        while (true) {
            Score score{};
            auto next_doc = std::numeric_limits<Document>::max();
            auto next_list = ranges.begin();
            for (auto it = ranges.begin(); it != ranges.end(); ++it) {
                if (extract_range(it)->empty()) {
                    continue;
                }
                if (extract_range(it)->begin()->document() == current_doc) {
                    score += score_fn(it);
                    extract_range(it)->advance();
                }
                if (not extract_range(it)->empty()
                    && extract_range(it)->begin()->document() < next_doc)
                {
                    next_doc = extract_range(it)->begin()->document();
                    next_list = it;
                }
            }
            acc.accumulate(current_doc, score);
            if (next_doc < std::numeric_limits<Document>::max()) {
                current_doc = next_doc;
                std::iter_swap(ranges.begin(), next_list);
            } else {
                break;
            }
        }
        return acc.sorted();
    }

}

/// Traverses scored posting lists in TAAT fashion.
///
/// \returns The top k results in an arbitrary order
template<class T>
// requires ScoredPostingList<T>
auto taat(gsl::span<const T> postings, std::ptrdiff_t collection_size, int k)
{
    using document_type = detail::document_type<decltype(*postings.begin())>;
    using score_type = detail::score_type<decltype(*postings.begin())>;
    std::vector<score_type> acc(collection_size);
    for (const auto& posting_list : postings) {
        for (const auto posting : posting_list) {
            acc[posting.document()] += posting.payload();
        }
    }
    return irk::aggregate_top_k<document_type, score_type>(
        std::begin(acc), std::end(acc), k);
}

/// Traverses unscored posting lists in TAAT fashion.
///
/// \returns The top k results in an arbitrary order
template<class T, class F>
// requires UnscoredPostingList<T> && TermScoreFn<F>
auto taat(
    gsl::span<const T> postings,
    gsl::span<const F> score_fns,
    std::ptrdiff_t collection_size,
    int k)
{
    using document_type = detail::document_type<decltype(*postings.begin())>;
    std::vector<double> acc(collection_size);
    for (auto&& [posting_list, score_fn] : iter::zip(postings, score_fns)) {
        for (const auto posting : posting_list) {
            auto doc = posting.document();
            acc[doc] += score_fn(doc, posting.payload());
        }
    }
    return irk::aggregate_top_k<document_type, double>(
        std::begin(acc), std::end(acc), k);
}

template<class T>
// requires ScoredPostingList<T>
auto daat(gsl::span<const T> postings, int k)
{
    using Iterator = decltype(std::cbegin(std::declval<T>()));
    using Range = irk::moving_range<Iterator>;
    using Score = detail::score_type<decltype(*postings.begin())>;
    using Document = detail::document_type<decltype(*postings.begin())>;

    std::vector<Range> ranges;
    for (const auto& posting_list : postings) {
        ranges.emplace_back(posting_list.begin(), posting_list.end());
    }

    return detail::daat<T, Iterator, Range, Document, Score>(
        postings,
        k,
        std::move(ranges),
        detail::moving_range_order,
        [](auto&& it) { return it; },
        [](auto&& it) { return it->begin()->payload(); });
}

template<class T, class F>
// requires UnscoredPostingList<T> && TermScoreFn<F>
auto daat(gsl::span<const T> postings, gsl::span<const F> score_fns, int k)
{
    using Iterator = decltype(std::cbegin(std::declval<T>()));
    using Range = std::pair<int, irk::moving_range<Iterator>>;
    using Score = double;
    using Document = detail::document_type<decltype(*postings.begin())>;

    std::vector<Range> ranges;
    irk::transform_ranges(
        iter::range(postings.size()),
        postings,
        std::back_inserter(ranges),
        [](auto idx, const auto& posting_list) {
            return std::make_pair(
                idx,
                irk::moving_range{posting_list.begin(), posting_list.end()});
        });

    return detail::daat<T, Iterator, Range, Document, Score>(
        postings,
        k,
        std::move(ranges),
        detail::pair_moving_range_order,
        [](auto&& it) { return &it->second; },
        [&](auto&& it) {
            return score_fns[it->first](
                it->second.begin()->document(), it->second.begin()->payload());
        });
}

void for_each_query(std::istream& input,
                    bool stem,
                    std::function<void(int, gsl::span<std::string const>)> f)
{
    int counter{0};
    for (const auto& query_line : irk::io::lines_from_stream(input)) {
        std::vector<std::string> terms;
        boost::split(terms, query_line, boost::is_any_of("\t "), boost::token_compress_on);
        f(counter++, terms);
    }
}

}  // namespace irk
