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

#include <vector>

#include <CLI/CLI.hpp>
#include <irkit/value.hpp>
#include <taily.hpp>

#include "cli.hpp"
#include "run_query.hpp"

using namespace irk::cli;
using irtl::value;
using taily::CollectionStatistics;
using taily::FeatureStatistics;

template<typename Index>
inline auto query_stats(Index const& index, std::vector<std::string> const& terms)
    -> std::vector<FeatureStatistics>
{
    const std::string ql{"ql"};
    auto means = value(index.score_mean(ql), "no means found");
    auto variances = value(index.score_var(ql), "no variances found");
    std::vector<FeatureStatistics> stats(terms.size());
    irk::transform_range(terms, std::begin(stats), [&](auto const& term) {
        if (auto id = index.term_id(term); id) {
            return FeatureStatistics{means[id.value()],
                                     variances[id.value()],
                                     index.term_collection_frequency(id.value())};
        } else {
            return FeatureStatistics{0, 0, 0};
        }
    });
    return stats;
}

inline void run_taily(irk::Index_Cluster const& cluster,
                      std::vector<std::string> const& terms,
                      int ntop,
                      std::optional<int> trec_id)
{
    CollectionStatistics global_stats{query_stats(cluster, terms), cluster.collection_size()};
    std::vector<CollectionStatistics> shard_stats;
    irk::transform_range(cluster.shards(), std::back_inserter(shard_stats), [&](auto const& shard) {
        return CollectionStatistics{query_stats(shard, terms), shard.collection_size()};
    });
    std::vector<double> scores = taily::score_shards(global_stats, shard_stats, ntop);

    if (trec_id) {
        int query = trec_id.value();
        for (auto&& [shard, score] : iter::enumerate(scores)) {
            std::cout << fmt::format("{}\t{}\t{}\n", query, shard, score);
        }
    } else {
        for (auto&& [shard, score] : iter::enumerate(scores)) {
            std::cout << fmt::format("{}\t{}\n", shard, score);
        }
    }
}

int main(int argc, char** argv)
{
    auto [app, args] = irk::cli::app("Query index cluster",
                                     index_dir_opt{},
                                     nostem_opt{},
                                     k_opt{},
                                     trec_id_opt{},
                                     terms_pos{optional});
    CLI11_PARSE(*app, argc, argv);
    boost::filesystem::path dir(args->index_dir);

    auto source = irk::Index_Cluster_Data_Source<irk::inverted_index_mapped_data_source>::from(dir);
    irk::Index_Cluster cluster{source};

    auto first_trecid = app->count("--trec-id") > 0u ? std::make_optional(args->trec_id)
                                                     : std::nullopt;
    if (not args->terms.empty()) {
        irk::cli::stem_if(not args->nostem, args->terms);
        run_taily(cluster, args->terms, args->k, first_trecid);
    } else {
        irk::run_queries(
            first_trecid,
            [&, k = args->k, nostem = args->nostem](auto const& current_trecid, auto& terms) {
                irk::cli::stem_if(not nostem, terms);
                run_taily(cluster, terms, k, current_trecid);
            });
    }
}
