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

#include <algorithm>

#include <irkit/movingrange.hpp>
#include <irkit/types.hpp>
#include <irkit/unionrange.hpp>
#include <irkit/utils.hpp>

namespace irk {

////! Returns top-*k* results, given vectors of posting lists and term weights.
//template<class Range, class Score>
//std::vector<pure_element_t<Range>> daat_or(
//    const std::vector<Range>& query_postings,
//    std::size_t k,
//    const std::vector<Score>& weights)
//{
//    using Posting = pure_element_t<Range>;
//    union_range postings_union(query_postings, weights);
//    top_k_accumulator<Posting> topk(k);
//    while (not postings_union.empty()) {
//        topk.accumulate(postings_union.next_doc());
//    }
//    return topk.sorted();
//}
//
////! Returns top-*k* results, given vectors of posting lists and term weights.
//template<class Range, class Score>
//std::vector<pure_element_t<Range>> wand(
//    const std::vector<Range>& query_postings,
//    std::size_t k,
//    const std::vector<Score>& weights)
//{
//    using Posting = pure_element_t<Range>;
//    union_range postings_union(query_postings, weights);
//    top_k_accumulator<Posting> topk(k);
//    while (not postings_union.empty()) {
//        topk.accumulate(postings_union.next_doc_wand(topk.threshold()));
//    }
//    return topk.sorted();
//}

}  // namespace irk
