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

#include <chrono>
#include <iostream>
#include <string>
#include <unordered_set>

#include <CLI/CLI.hpp>
#include <boost/filesystem.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <tbb/task_scheduler_init.h>

#include <irkit/index.hpp>
#include <irkit/index/cluster.hpp>
#include <irkit/index/score.hpp>
#include <irkit/index/source.hpp>
#include <irkit/timer.hpp>
#include <irkit/value.hpp>
#include "cli.hpp"

namespace fs = boost::filesystem;
using source_type = irk::inverted_index_mapped_data_source;

using namespace irk::cli;
using irk::index::Properties;

class Scoreable_Index {
public:
    template<class Source, class Index, class ScoreTag>
    explicit Scoreable_Index(std::shared_ptr<Source> source, Index index, ScoreTag tag)
        : self_(std::make_shared<Impl<Source, Index, ScoreTag>>(
              std::move(source), std::move(index), tag))
    {}

    [[nodiscard]] static Scoreable_Index
    from(boost::filesystem::path const& dir, std::string const& score_name)
    {
        auto props = Properties::read(dir);
        if (props.shard_count) {
            auto source = irk::Index_Cluster_Data_Source<
                irk::inverted_index_mapped_data_source>::from(dir);
            if (score_name == "bm25") {
                return Scoreable_Index(source, irk::Index_Cluster(source), irk::score::bm25);
            } else if (score_name == "ql") {
                return Scoreable_Index(
                    source, irk::Index_Cluster(source), irk::score::query_likelihood);
            } else {
                throw "unknown scorer";
            }
        } else {
            auto source = std::make_shared<irk::inverted_index_mapped_data_source>(dir);
            *source = irtl::value(irk::inverted_index_mapped_data_source::from(dir));
            if (score_name == "bm25") {
                return Scoreable_Index(
                    source, irk::inverted_index_view(source.get()), irk::score::bm25);
            } else if (score_name == "ql") {
                return Scoreable_Index(
                    source, irk::inverted_index_view(source.get()), irk::score::query_likelihood);
            } else {
                throw "unknown scorer";
            }
        }
    }

    nonstd::expected<void, std::string> calc_score_stats() { return self_->calc_score_stats(); }

private:
    struct Scoreable
    {
        Scoreable() = default;
        Scoreable(Scoreable const&) = default;
        Scoreable(Scoreable&&) noexcept = default;
        Scoreable& operator=(Scoreable const&) = default;
        Scoreable& operator=(Scoreable&&) noexcept = default;
        virtual ~Scoreable() = default;
        virtual nonstd::expected<void, std::string> calc_score_stats() = 0;
    };

    template<class Source, class Index, class ScoreTag>
    class Impl : public Scoreable {
    public:
        explicit Impl(std::shared_ptr<Source> source, Index index, ScoreTag)
            : source_(std::move(source)), index_(std::move(index))
        {}
        nonstd::expected<void, std::string> calc_score_stats() override
        {
            for (auto const& shard : index_.shards()) {
                using term_id_type = typename Index::term_id_type;
                irk::index::detail::ScoreStatsFn fn{shard.dir(), std::string{ScoreTag{}}};
                std::vector<term_id_type> term_ids(shard.term_count());
                std::iota(term_ids.begin(), term_ids.end(), term_id_type{0});
                fn(term_ids, [&](term_id_type id) {
                    return shard.postings(id).scored(shard.term_scorer(id, ScoreTag{}));
                });
            }
            return nonstd::expected<void, std::string>();
        }

    private:
        std::shared_ptr<Source> source_;
        Index index_;
    };

private:
    std::shared_ptr<Scoreable> self_;
};

int main(int argc, char** argv)
{
    auto [app, args] = irk::cli::app("Calculate score statistics",
                                     index_dir_opt{},
                                     threads_opt{},
                                     score_function_opt{with_default<std::string>{"bm25"}});
    CLI11_PARSE(*app, argc, argv);

    tbb::task_scheduler_init init(args->threads);
    auto log = spdlog::stderr_color_mt("console");
    auto index = Scoreable_Index::from(args->index_dir, args->score_function);
    log->info("Calculating score statistics using {} threads", args->threads);
    irk::run_with_timer<std::chrono::milliseconds>(
        [&]() {
            if (auto res = index.calc_score_stats(); not res) {
                log->error("Fatal error: {}", res.error());
            }
        },
        irk::cli::log_finished{log});
    return 0;
}
