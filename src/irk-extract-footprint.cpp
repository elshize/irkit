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

#include <iostream>
#include <numeric>

#include <CLI/CLI.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/te.hpp>
#include <irkit/algorithm/accumulate.hpp>
#include <irkit/algorithm/group_by.hpp>
#include <irkit/algorithm/query.hpp>
#include <irkit/coding/vbyte.hpp>
#include <irkit/index.hpp>
#include <irkit/index/source.hpp>
#include <irkit/index/types.hpp>
#include <irkit/parsing/stemmer.hpp>
#include <irkit/shard_container.hpp>

#include "cli.hpp"
#include "run_query.hpp"

using boost::filesystem::path;
using irk::Inverted_Index_Mapped_Source;
using irk::inverted_index_view;
using irk::cli::optional;
using irk::index::term_id_t;
using irk::Query_Engine;

int main(int argc, char** argv)
{
    auto [app, args] = irk::cli::app("Extract query footprint for queries",
                                     irk::cli::index_dir_opt{},
                                     irk::cli::nostem_opt{},
                                     irk::cli::k_opt{},
                                     irk::cli::score_function_opt{});
    CLI11_PARSE(*app, argc, argv);
    auto index = irk::Shard_Container::from(args->index_dir,
                                            gsl::make_span(&args->score_function, 1));
    std::vector<Query_Engine> shard_engines;
    irk::transform_range(
        index.shards(),
        std::back_inserter(shard_engines),
        [sf = args->score_function](auto const& shard) {
            return irk::Query_Engine::from(
                shard, false, sf, irk::Traversal_Type::TAAT, std::optional<int>{}, "null");
        });
    std::cout << "query,shard,footprint\n";
    irk::for_each_query(std::cin, not args->nostem, [&, k = args->k](int qid, auto terms) {
        int shard_id{0};
        for (auto const& shard : index.shards()) {
            auto results = shard_engines[shard_id].run_query(terms, k);
            auto top_documents = results.top_documents();
            std::sort(top_documents.begin(), top_documents.end());
            std::unordered_map<irk::index::document_t, int> term_counts(results.size());
            auto document_lists = irk::query_documents(shard, terms);
            auto iterators = irk::begins(gsl::make_span(document_lists));
            auto ends = irk::ends(gsl::make_span(document_lists));
            for (auto doc : top_documents) {
                for (auto&& [iterator, end] : iter::zip(iterators, ends)) {
                    iterator.advance_to(doc);
                    if (iterator != end && *iterator == doc) {
                        term_counts[doc] += 1;
                    }
                }
            };
            double footprint = 0.0;
            for ([[maybe_unused]] auto&& [doc, count] : term_counts) {
                footprint += static_cast<double>(count) / terms.size();
            }
            footprint /= term_counts.size();
            std::cout << fmt::format("{},{},{}\n", qid, shard_id, footprint);
        }
    });
    return 0;
}
