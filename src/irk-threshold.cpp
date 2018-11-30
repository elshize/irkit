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

#include <CLI/CLI.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <cppitertools/itertools.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <taily.hpp>

#include <irkit/algorithm.hpp>
#include <irkit/index.hpp>
#include <irkit/index/source.hpp>
#include <irkit/parsing/stemmer.hpp>
#include <irkit/threshold.hpp>
#include <irkit/utils.hpp>
#include <irkit/value.hpp>
#include "cli.hpp"

using boost::filesystem::path;
using irk::index::document_t;
using std::uint32_t;
using namespace irk;

void threshold(
    const irk::inverted_index_view& index,
    std::vector<std::string>& terms,
    int topk,
    bool nostem,
    const std::string& scorer)
{
    irk::cli::stem_if(not nostem, terms);

    if (scorer[0] == '*') {
        auto postings = irk::cli::postings_on_fly(terms, index, scorer);
        auto threshold =
            irk::compute_threshold(postings.begin(), postings.end(), topk);
        std::cout << threshold << '\n';
        return;
    }
    auto documents = irk::query_documents(index, terms);
    auto scores = irk::query_scores(index, terms);
    auto threshold = irk::compute_threshold(
        documents.begin(), documents.end(), scores.begin(), scores.end(), topk);
    std::cout << threshold << '\n';
}

auto estimate_taily(
    const irk::inverted_index_view& index,
    std::vector<std::string>& terms,
    int topk,
    const std::string& scorer)
{
    std::vector<taily::FeatureStatistics> feature_stats(terms.size());
    auto means = irtl::value(index.score_mean(scorer), "failed to fetch means");
    auto vars = irtl::value(index.score_mean(scorer),
                            "failed to fetch variances");
    irk::transform_range(
        terms, std::begin(feature_stats), [&](const auto& term) {
            if (auto id = index.term_id(term); id.has_value()) {
                if (irk::cli::on_fly(scorer)) {
                    auto scores = iter::imap(
                        [](const auto& posting) { return posting.score(); },
                        irk::cli::postings_on_fly(term, index, scorer));
                    return taily::FeatureStatistics::from_features(scores);
                }
                return taily::FeatureStatistics{
                    static_cast<double>(means[*id]),
                    static_cast<double>(vars[*id]),
                    index.term_collection_frequency(*id)};
            }
            return taily::FeatureStatistics{0, 0, 0};
        });
    taily::CollectionStatistics stats{
        std::move(feature_stats), index.collection_size()};
    return taily::estimate_cutoff(stats, topk);
}

void estimate(
    const irk::inverted_index_view& index,
    std::vector<std::string>& terms,
    int topk,
    bool nostem,
    cli::ThresholdEstimator estimate_method,
    const std::string& scorer)
{
    irk::cli::stem_if(not nostem, terms);
    if (irk::cli::on_fly(scorer)) {
        double threshold;
        switch (estimate_method) {
        case cli::ThresholdEstimator::taily: {
            std::string name(std::next(scorer.begin()), scorer.end());
            threshold = estimate_taily(index, terms, topk, name);
            break;
        }
        default:
            throw std::domain_error(
                "ThresholdEstimator: non-exhaustive switch");
        }
        auto postings = irk::cli::postings_on_fly(terms, index, scorer);
        auto k = irk::compute_topk(postings.begin(), postings.end(), threshold);
        std::cout << threshold << '\t' << k << '\n';
        return;
    }
    inverted_index_view::score_type threshold;
    switch (estimate_method) {
    case cli::ThresholdEstimator::taily:
        //threshold = estimate_taily(index, terms, topk, scorer);
        throw std::domain_error("taily not supported yet");
        break;
    default:
        throw std::domain_error("ThresholdEstimator: non-exhaustive switch");
    }
    auto documents = irk::query_documents(index, terms);
    auto scores = irk::query_scores(index, terms);
    auto k = irk::compute_topk(
        documents.begin(),
        documents.end(),
        scores.begin(),
        scores.end(),
        threshold);
    std::cout << threshold << '\t' << k << '\n';
}

int main(int argc, char** argv)
{
    std::optional<cli::ThresholdEstimator> estimate_method;
    auto [app, args] = irk::cli::app(
        "Compute or estimate top-k threshold",
        irk::cli::index_dir_opt{},
        irk::cli::nostem_opt{},
        irk::cli::score_function_opt(
            irk::cli::with_default<std::string>{"bm25"}),
        irk::cli::k_opt{},
        irk::cli::terms_pos{irk::cli::optional});
    cli::add_threshold_estimator(
        *app,
        "-e,--estimate",
        estimate_method,
        "Method to estimate threshold. By default, it will be computed "
        "exactly.",
        false);
    CLI11_PARSE(*app, argc, argv);

    boost::filesystem::path dir(args->index_dir);
    std::vector<std::string> scores;
    if (args->score_function[0] != '*') {
        scores.push_back(args->score_function);
    }
    irk::inverted_index_view index(
        irtl::value(irk::Inverted_Index_Mapped_Source::from(dir, scores)));

    if (not args->terms.empty()) {
        if (estimate_method.has_value()) {
            estimate(
                index,
                args->terms,
                args->k,
                args->nostem,
                estimate_method.value(),
                args->score_function);
            return 0;
        }
        threshold(
            index, args->terms, args->k, args->nostem, args->score_function);
    }
    else {
        for (const auto& query_line : irk::io::lines_from_stream(std::cin)) {
            std::vector<std::string> terms;
            boost::split(
                terms,
                query_line,
                boost::is_any_of("\t "),
                boost::token_compress_on);
            if (estimate_method.has_value()) {
                estimate(
                    index,
                    terms,
                    args->k,
                    args->nostem,
                    estimate_method.value(),
                    args->score_function);
                continue;
            }
            threshold(
                index, terms, args->k, args->nostem, args->score_function);
        }
    }
    return 0;
}
