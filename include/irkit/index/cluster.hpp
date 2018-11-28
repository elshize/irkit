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
class Basic_Index_Cluster {
public:
    using size_type = typename InvertedIndex::size_type;
    using document_type = typename InvertedIndex::document_type;
    using score_type = typename InvertedIndex::score_type;
    using term_id_type = typename InvertedIndex::term_id_type;
    using frequency_table_type = typename InvertedIndex::frequency_table_type;

    template<class ShardSource>
    constexpr Basic_Index_Cluster(
        std::shared_ptr<Index_Cluster_Data_Source<ShardSource> const> source)
        : properties_(source->properties()),
          reverse_mapping_(source->reverse_mapping()),
          term_collection_frequencies_(source->term_collection_frequencies_view()),
          term_collection_occurrences_(source->term_collection_occurrences_view()),
          term_map_(std::move(load_lexicon(source->term_map_view())))
    {
        for (const auto& shard_source : source->shards()) {
            shards_.emplace_back(&shard_source);
        }
    }

    [[nodiscard]] auto shard_count() const { return irk::sgnd(shards_.size()); }
    [[nodiscard]] auto shard(ShardId shard) const -> InvertedIndex const& { return shards_[shard]; }
    [[nodiscard]] auto const& shards() const { return shards_; }
    [[nodiscard]] auto const& reverse_mapping() const { return reverse_mapping_; }
    [[nodiscard]] auto const& reverse_mapping(ShardId shard) const
    {
        return reverse_mapping_[shard];
    }

    [[nodiscard]] auto term_id(std::string const& term) const -> std::optional<term_id_type>
    {
        return term_map_.index_at(term);
    }

    [[nodiscard]] auto term(term_id_type const& id) const -> std::string
    {
        return term_map_.key_at(id);
    }

    [[nodiscard]] auto
    term_scorer(InvertedIndex const& shard, term_id_type term_id, score::bm25_tag) const
    {
        return score::BM25TermScorer{shard,
                                     score::bm25_scorer(term_collection_frequencies_[term_id],
                                                        properties_.document_count,
                                                        properties_.avg_document_size)};
    }

    [[nodiscard]] auto
    term_scorer(InvertedIndex const& shard, term_id_type term_id, score::query_likelihood_tag) const
    {
        return score::QueryLikelihoodTermScorer{
            shard,
            score::query_likelihood_scorer(term_collection_occurrences_[term_id],
                                           properties_.occurrences_count,
                                           properties_.max_document_size)};
    }

private:
    index::Properties properties_;
    Vector<ShardId, Vector<index::document_t>> const& reverse_mapping_;
    Vector<ShardId, InvertedIndex> shards_;
    frequency_table_type term_collection_frequencies_;
    frequency_table_type term_collection_occurrences_;
    lexicon<hutucker_codec<char>, memory_view> term_map_;
};

using Index_Cluster = Basic_Index_Cluster<inverted_index_view>;

}  // namespace irk
