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

#include "cli.hpp"
#include "run_query.hpp"

using boost::filesystem::path;
using irk::Inverted_Index_Mapped_Source;
using irk::inverted_index_view;
using irk::Query_Engine;
using irk::cli::optional;
using irk::index::term_id_t;
using namespace irk::cli;

struct Shard_Container : boost::te::poly<Shard_Container> {
    using boost::te::poly<Shard_Container>::poly;

    [[nodiscard]] auto shards() const -> gsl::span<irk::inverted_index_view const>
    {
        return boost::te::call<gsl::span<irk::inverted_index_view const>>(
            [](auto const& self) { return self.shards(); }, *this);
    }

    [[nodiscard]] static Shard_Container from(boost::filesystem::path const& dir)
    {
        auto props = irk::index::Properties::read(dir);
        if (props.shard_count) {
            auto source = irk::Index_Cluster_Data_Source<irk::Inverted_Index_Mapped_Source>::from(
                dir);
            return irk::Index_Cluster(source);
        } else {
            auto source = irtl::value(irk::Inverted_Index_Mapped_Source::from(dir));
            return irk::inverted_index_view(source);
        }
    }
};

int main(int argc, char** argv)
{
    auto [app, args] = irk::cli::app("Extract posting counts",
                                     index_dir_opt{},
                                     nostem_opt{},
                                     k_opt{},
                                     score_function_opt{with_default<std::string>{"bm25"}});
    CLI11_PARSE(*app, argc, argv);
    auto index = Shard_Container::from(args->index_dir);
    std::vector<Query_Engine> shard_engines;
    irk::transform_range(
        index.shards(),
        std::back_inserter(shard_engines),
        [sf = args->score_function](auto const& shard) {
            return irk::Query_Engine::from(
                shard, false, sf, irk::Traversal_Type::TAAT, std::optional<int>{}, "null");
        });
    std::cout << "query,shard,rank,document,score\n";
    irk::for_each_query(std::cin, not args->nostem, [&, k = args->k](int qid, auto terms) {
        for (auto shard_id : iter::range(shard_engines.size())) {
            shard_engines[shard_id].run_query(terms, k).print([&](int rank,
                                                                  auto document,
                                                                  auto score) {
                std::cout << fmt::format("{},{},{},{},{}\n", qid, shard_id, rank, document, score);
            });
        }
    });
    return 0;
}
