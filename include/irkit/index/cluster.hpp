// MIT License
//
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

#include <irkit/index.hpp>
#include <irkit/index/source.hpp>

namespace irk {

template<class InvertedIndex>
class basic_index_cluster {
public:
    template<class ShardSource>
    basic_index_cluster(
        std::shared_ptr<const index_cluster_data_source<ShardSource>> source)
    {
        for (const auto& shard_source : source->shards()) {
            shards_.emplace_back(&shard_source);
        }
    }

    const InvertedIndex shard(ShardId shard) const { return shards_[shard]; }
    const auto& shards() const { return shards_; }

private:
    irk::vmap<ShardId, InvertedIndex> shards_;
};

using index_cluster = basic_index_cluster<inverted_index_view>;

}  // namespace irk
