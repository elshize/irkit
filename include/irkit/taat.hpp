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

template<class T>
struct block_accumulator_vector {
    int block_size = 0;
    std::vector<T> accumulators{};
    std::vector<T> max_values{};
    block_accumulator_vector(int count, int block_size)
        : block_size(block_size),
          accumulators(count, 0),
          max_values((count + block_size - 1) / block_size)
    {}

    struct reference {
        T& accumulator;
        T& block_max;
        reference& operator=(const T& v)
        {
            accumulator = v;
            block_max = std::max(block_max, v);
            return *this;
        }
        reference& operator+=(const T& v)
        {
            accumulator += v;
            block_max = std::max(block_max, accumulator);
            return *this;
        }
    };

    reference operator[](std::ptrdiff_t index)
    {
        return reference{accumulators[index], max_values[index / block_size]};
    }
};

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

template<class Key, class Value>
std::vector<std::pair<Key, Value>>
aggregate_top_k(const block_accumulator_vector<Value>& accumulators, int k)
{
    irk::top_k_accumulator<Key, Value> top(k);
    for (std::size_t block = 0; block < accumulators.max_values.size(); ++block)
    {
        if (accumulators.max_values[block] < top.threshold()) { continue; }
        std::size_t begin = block * accumulators.block_size;
        std::size_t end = std::min((block + 1) * accumulators.block_size,
            accumulators.accumulators.size());
        for (Key idx = begin; idx < end; ++idx) {
            top.accumulate(idx, accumulators.accumulators[idx]);
        }
    }
    return top.sorted();
}

};  // namespace irk
