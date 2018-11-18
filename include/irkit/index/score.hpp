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
#include <pstl/algorithm>
#include <pstl/execution>
#include <tbb/blocked_range.h>
#include <tbb/parallel_reduce.h>

#include <irkit/algorithm/max.hpp>
#include <irkit/coding/copy.hpp>
#include <irkit/index.hpp>
#include <irkit/io.hpp>

namespace ts = type_safe;

namespace irk::index {

namespace detail {

    using boost::accumulators::stats;
    using boost::accumulators::tag::max;
    using boost::accumulators::tag::mean;
    using boost::accumulators::tag::variance;
    using boost::accumulators::accumulator_set;

    struct StatTuple {
        double max{};
        double mean{};
        double var{};
    };

    template<class Fun>
    std::vector<float>
    unzip(const std::vector<StatTuple>& stat_vector, Fun field)
    {
        std::vector<float> unzipped(stat_vector.size());
        std::transform(
            std::execution::par_unseq,
            stat_vector.begin(),
            stat_vector.end(),
            unzipped.begin(),
            field);
        return unzipped;
    }

    void write_table(const std::vector<float>& vec, const path& file)
    {
        std::ofstream of(file.c_str());
        io::write_vector(vec, of);
    }

    class ScoreStatsFn {
    public:
        ScoreStatsFn(fs::path dir_path, std::string name)
            : dir_(std::move(dir_path)),
              max_scores_path_(dir_ / (name + ".max")),
              mean_scores_path_(dir_ / (name + ".mean")),
              var_scores_path_(dir_ / (name + ".var")),
              name_(std::move(name))
        {}

        template<class RandomAccessRng, class UnaryFun>
        void
        operator()(const RandomAccessRng& term_ids, UnaryFun scored_postings)
        {
            std::vector<StatTuple> stat_vec(term_ids.size());
            std::transform(
                std::execution::par_unseq,
                term_ids.begin(),
                term_ids.end(),
                stat_vec.begin(),
                [&](auto term_id) {
                    accumulator_set<float, stats<mean, variance, max>> acc;
                    for (const auto& posting : scored_postings(term_id)) {
                        acc(posting.score());
                    }
                    return StatTuple{boost::accumulators::max(acc),
                                     boost::accumulators::mean(acc),
                                     boost::accumulators::variance(acc)};
                });

            std::vector<float> max_vec =
                unzip(stat_vec, [](const auto& tuple) { return tuple.max; });
            std::vector<float> mean_vec =
                unzip(stat_vec, [](const auto& tuple) { return tuple.mean; });
            std::vector<float> var_vec =
                unzip(stat_vec, [](const auto& tuple) { return tuple.var; });

            write_table(max_vec, max_scores_path_);
            write_table(mean_vec, mean_scores_path_);
            write_table(var_vec, var_scores_path_);
        }

    private:
        boost::filesystem::path dir_;
        boost::filesystem::path max_scores_path_;
        boost::filesystem::path mean_scores_path_;
        boost::filesystem::path var_scores_path_;
        std::string name_;
    };

    template<class ScoreTag, class DataSourceT>
    class ScoreFn {
    public:
        ScoreFn(fs::path dir_path, std::string type_name, int bits)
            : bits(bits),
              name(fmt::format("{}-{}", type_name, bits)),
              dir(std::move(dir_path)),
              scores_path(dir / (name + ".scores")),
              score_offsets_path(dir / (name + ".offsets")),
              score_max_path(dir / (name + ".maxscore")),
              type_name(std::move(type_name))
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
                    auto scorer = index.term_scorer(term, ScoreTag{});
                    for (const auto& posting : index.postings(term)) {
                        double score =
                            scorer(posting.document(), posting.payload());
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
            auto type = index::QuantizationProperties::parse_type(type_name);
            if (not type) {
                return nonstd::make_unexpected(type.error());
            }
            auto props = index::Properties::read(dir);

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
            std::vector<std::size_t> offsets;
            std::vector<std::uint32_t> max_scores;
            std::vector<std::uint32_t> exp_scores;
            std::vector<std::uint32_t> var_scores;
            offsets.reserve(index.term_count());
            max_scores.reserve(index.term_count());

            if (log) {
                log->info("Calculating max score");
            }
            const auto [min_score, max_score] = min_max(index);
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
                auto scorer = index.term_scorer(term_id, ScoreTag{});
                for (const auto& posting : index.postings(term_id)) {
                    double score =
                        scorer(posting.document(), posting.payload());
                    list_builder.add(quantize(score));
                    acc(score);
                }
                max_scores.push_back(ba::max(acc));
                offset += list_builder.write(sout);
            }
            const auto offset_table = irk::build_offset_table<>(offsets);
            offout << offset_table;
            const auto maxscore_table =
                irk::build_compact_table<uint32_t>(max_scores);
            maxout << maxscore_table;

            index::QuantizationProperties qprops;
            qprops.type = type.value();
            qprops.nbits = bits;
            qprops.min = min_score;
            qprops.max = max_score;
            props.quantized_scores[name] = qprops;
            index::Properties::write(props, dir);

            return nonstd::expected<void, std::string>();
        }

    private:
        const int bits;
        const std::string name;
        const fs::path dir;
        const fs::path scores_path;
        const fs::path score_offsets_path;
        const fs::path score_max_path;
        const std::string type_name;
    };

}  // namespace detail

template<class ScoreTag, class DataSourceT>
nonstd::expected<void, std::string>
score_index(const fs::path& dir_path, unsigned int bits)
{
    std::string name(ScoreTag{});
    detail::ScoreFn<ScoreTag, DataSourceT> fn(dir_path, name, bits);
    fn();
    return nonstd::expected<void, std::string>();
}

template<class ScoreTag, class Index, class DataSourceT>
nonstd::expected<void, std::string>
calc_score_stats(const fs::path& dir_path)
{
    auto source = DataSourceT::from(dir_path);
    if (not source) {
        return source.get_unexpected();
    }
    Index index(&source.value());
    detail::ScoreStatsFn fn{dir_path, std::string{ScoreTag{}}};
    std::vector<term_id_t> term_ids(index.term_count());
    std::iota(term_ids.begin(), term_ids.end(), term_id_t{0});
    fn(term_ids, [&](term_id_t id) {
        return index.postings(id).scored(index.term_scorer(id, ScoreTag{}));
    });
    return nonstd::expected<void, std::string>();
}

}  // namespace irk::index
