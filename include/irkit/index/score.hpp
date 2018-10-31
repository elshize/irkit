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

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <nonstd/expected.hpp>
#include <tbb/blocked_range.h>
#include <tbb/parallel_reduce.h>

#include <irkit/algorithm/max.hpp>
#include <irkit/index.hpp>

namespace ts = type_safe;

namespace irk::index {

namespace detail {

    template<class Scorer, class DataSourceT>
    class ScoreFn {
    public:
        ScoreFn(fs::path dir_path, const std::string& name, int bits)
            : bits(bits),
              dir(std::move(dir_path)),
              scores_path(dir / (name + ".scores")),
              score_offsets_path(dir / (name + ".offsets")),
              score_max_path(dir / (name + ".maxscore")),
              score_exp_path(dir / (name + ".expscore")),
              score_var_path(dir / (name + ".varscore"))
        {}

        std::pair<double, double> min_max(const inverted_index_view& index)
        {
            std::pair<double, double> initial = {
                std::numeric_limits<double>::max(),
                std::numeric_limits<double>::lowest()};
            auto reduce_pair = [](auto lhs, auto rhs) {
                auto p = std::make_pair<double, double>(
                    irk::min_val(lhs.first, rhs.first),
                    irk::max_val(lhs.second, rhs.second));
                return p;
            };
            auto reduce_range = [&index, &reduce_pair](
                                    const tbb::blocked_range<term_id_t>& range,
                                    const auto& init) {
                auto min = std::numeric_limits<double>::max();
                auto max = std::numeric_limits<double>::lowest();
                for (auto term = range.begin(); term != range.end(); ++term) {
                    auto scorer = index.term_scorer<Scorer>(term);
                    for (const auto& posting : index.postings(term)) {
                        double score = scorer(
                            posting.payload(),
                            index.document_size(posting.document()));
                        min = irk::min_val(min, score);
                        max = irk::max_val(max, score);
                    }
                }
                return reduce_pair(std::make_pair(min, max), init);
            };
            auto [min, max] = tbb::parallel_reduce(
                tbb::blocked_range<term_id_t>(0, index.term_count()),
                initial,
                reduce_range,
                reduce_pair);
            return {irk::min_val(min, 0.0), irk::max_val(max, 0.0)};
        }

        nonstd::expected<void, std::string> operator()()
        {
            namespace ba = boost::accumulators;
            using stat_accumulator = ba::accumulator_set<
                double,
                ba::stats<ba::tag::mean, ba::tag::variance, ba::tag::max>>;
            auto source = DataSourceT::from(dir);
            if (not source) {
                return source.get_unexpected();
            }
            inverted_index_view index(&source.value());

            auto log = spdlog::get("score");

            int64_t offset = 0;
            std::ofstream sout(scores_path.c_str());
            std::ofstream offout(score_offsets_path.c_str());
            std::ofstream maxout(score_max_path.c_str());
            std::ofstream expout(score_exp_path.c_str());
            std::ofstream varout(score_var_path.c_str());
            std::vector<std::size_t> offsets;
            std::vector<std::uint32_t> max_scores;
            std::vector<std::uint32_t> exp_scores;
            std::vector<std::uint32_t> var_scores;
            offsets.reserve(index.term_count());
            max_scores.reserve(index.term_count());
            exp_scores.reserve(index.term_count());
            var_scores.reserve(index.term_count());

            if (log) {
                log->info("Calculating max score");
            }
            auto [min_score, max_score] = min_max(index);
            if (log) {
                log->info("Max score: {}; Min score: {}", max_score, min_score);
            }

            LinearQuantizer quantize(
                RealRange(min_score, max_score),
                IntegralRange(1, (1u << bits) - 1u));

            for (term_id_t term_id = 0; term_id < index.terms().size();
                 term_id++) {
                offsets.push_back(offset);
                irk::index::block_list_builder<
                    std::uint32_t,
                    stream_vbyte_codec<std::uint32_t>,
                    false>
                    list_builder(index.skip_block_size());
                stat_accumulator acc;
                auto scorer = index.term_scorer<Scorer>(term_id);
                for (const auto& posting : index.postings(term_id)) {
                    double score = scorer(
                        posting.payload(),
                        index.document_size(posting.document()));
                    list_builder.add(quantize(score));
                    acc(score);
                }
                max_scores.push_back(ba::max(acc));
                exp_scores.push_back(0);
                var_scores.push_back(0);
                offset += list_builder.write(sout);
            }
            auto offset_table = irk::build_offset_table<>(offsets);
            offout << offset_table;
            auto maxscore_table =
                irk::build_compact_table<uint32_t>(max_scores);
            maxout << maxscore_table;
            auto expscore_table =
                irk::build_compact_table<uint32_t>(exp_scores);
            expout << expscore_table;
            auto varscore_table =
                irk::build_compact_table<uint32_t>(var_scores);
            varout << varscore_table;
            return nonstd::expected<void, std::string>();
        }

    private:
        const int bits;
        const fs::path dir;
        const fs::path scores_path;
        const fs::path score_offsets_path;
        const fs::path score_max_path;
        const fs::path score_exp_path;
        const fs::path score_var_path;
    };

}  // namespace detail

template<class Scorer, class DataSourceT>
nonstd::expected<void, std::string>
score_index(const fs::path& dir_path, unsigned int bits)
{
    std::string name(typename Scorer::tag_type{});
    detail::ScoreFn<Scorer, DataSourceT> fn(dir_path, name, bits);
    fn();
    return nonstd::expected<void, std::string>();
}

}
