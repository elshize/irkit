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

#include <irkit/algorithm/accumulate.hpp>
#include <irkit/assert.hpp>
#include <irkit/index/posting_list.hpp>
#include <irkit/utils.hpp>

#include <vector>

namespace irk {

template<typename DocumentIterator, typename ScoreIterator>
auto compute_threshold(
    DocumentIterator first_document_list,
    DocumentIterator last_document_list,
    ScoreIterator first_score_list,
    ScoreIterator last_score_list,
    int topk)
{
    auto numlists = std::distance(first_document_list, last_document_list);
    EXPECTS(numlists == std::distance(first_score_list, last_score_list));
    using posting_list = posting_list_view<
        std::remove_reference_t<decltype(*first_document_list)>,
        std::remove_reference_t<decltype(*first_score_list)>>;
    std::vector<posting_list> posting_lists;
    std::transform(
        first_document_list,
        last_document_list,
        first_score_list,
        std::back_inserter(posting_lists),
        [](const auto& doclist, const auto& scorelist) {
            return posting_list(doclist, scorelist);
        });
    return compute_threshold(posting_lists.begin(), posting_lists.end(), topk);
}

template<typename DocumentType, typename ScoreType, typename Iter>
ScoreType compute_threshold(
    Iter first_posting_list,
    Iter last_posting_list,
    top_k_accumulator<DocumentType, ScoreType>& acc)
{
    ScoreType score(0);
    auto postings = merge(first_posting_list, last_posting_list);
    auto pos = postings.begin();
    auto end = postings.end();
    while (pos != end) {
        auto current_id = pos->document();
        std::tie(score, pos) = accumulate_while(
            pos,
            end,
            ScoreType(0),
            [current_id](const auto& posting) {
                return posting.document() == current_id;
            },
            [](const auto& acc, const auto& posting) {
                return acc + posting.payload();
            });
        acc.accumulate(current_id, score);
    }
    return acc.threshold();
}

template<typename Iter>
auto compute_threshold(
    Iter first_posting_list, Iter last_posting_list, int topk)
{
    using document_type =
        std::decay_t<decltype(first_posting_list->begin()->document())>;
    using score_type =
        std::decay_t<decltype(first_posting_list->begin()->payload())>;
    top_k_accumulator<document_type, score_type> acc(topk);
    return compute_threshold<document_type, score_type, Iter>(
        first_posting_list, last_posting_list, acc);
}

}  // namespace irk
