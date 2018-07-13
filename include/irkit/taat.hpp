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

//! \file taat.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <vector>

#include <irkit/index/types.hpp>
#include <irkit/utils.hpp>

namespace irk {

template<class DocumentList, class PayloadList, class AccumulatorVec>
void accumulate(const DocumentList& documents,
    const PayloadList& payloads,
    AccumulatorVec& accumulators)
{
    auto dit = std::begin(documents);
    auto dend = std::end(documents);
    auto pit = std::begin(payloads);
    auto pend = std::end(payloads);
    for (; dit != dend; ++dit, ++pit) { accumulators[*dit] += *pit; }
}

template<class PostingList, class AccumulatorVec>
void accumulate(const PostingList& postings, AccumulatorVec& accumulators)
{
    for (const auto posting : postings)
    { accumulators[posting.document()] += posting.payload(); }
}

template<class PostingLists, class AccumulatorVec>
void taat(const PostingLists& posting_lists, AccumulatorVec& accumulators)
{
    for (const auto& posting_list : posting_lists)
    { accumulate(posting_list, accumulators); }
}

template<class Key, class Value, class AccumulatorVec>
std::vector<std::pair<Key, Value>>
aggregate_top_k(const AccumulatorVec& accumulators, int k)
{
    irk::top_k_accumulator<Key, Value> top(k);
    Key key = 0;
    for (const auto& value : accumulators) {
        top.accumulate(key++, value);
    }
    return top.sorted();
}

};  // namespace irk
