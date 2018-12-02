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
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <memory>
#include <vector>

#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <tbb/task_scheduler_init.h>

#include <irkit/algorithm/query.hpp>
#include <irkit/compacttable.hpp>
#include <irkit/daat.hpp>
#include <irkit/index/types.hpp>
#include <irkit/memoryview.hpp>
#include <irkit/parsing/stemmer.hpp>
#include <irkit/query_engine.hpp>
#include <irkit/score.hpp>
#include <irkit/taat.hpp>
#include <irkit/timer.hpp>

namespace irk::cli {

using document_t = index::document_t;

class docmap {
public:
    docmap(std::vector<document_t> doc2rank, std::vector<document_t> rank2doc);
    docmap() = default;
    docmap(const docmap&) = default;
    docmap(docmap&&) noexcept = default;
    docmap& operator=(const docmap&) = default;
    docmap& operator=(docmap&&) noexcept = default;
    ~docmap() noexcept = default;
    inline document_t rank(document_t doc) const;
    inline document_t doc(document_t rank) const;
    static docmap from_files(const std::string& files_prefix);
    const std::vector<document_t>& doc2rank() const;
    const std::vector<document_t>& rank2doc() const;

private:
    std::vector<document_t> doc2rank_;
    std::vector<document_t> rank2doc_;
};

docmap::docmap(
    std::vector<document_t> doc2rank, std::vector<document_t> rank2doc)
    : doc2rank_(std::move(doc2rank)), rank2doc_(std::move(rank2doc))
{}

inline document_t docmap::rank(document_t doc) const
{
    return doc2rank_[doc];
}

inline const std::vector<document_t>& docmap::doc2rank() const
{
    return doc2rank_;
}

inline const std::vector<document_t>& docmap::rank2doc() const
{
    return rank2doc_;
}

inline document_t docmap::doc(document_t rank) const
{
    return rank2doc_[rank];
}

inline docmap docmap::from_files(const std::string& files_prefix)
{
    using document_table_type =
        irk::compact_table<document_t, vbyte_codec<document_t>, memory_view>;
    auto [d2r_mem, d2r_view] = irk::make_memory_view(
        boost::filesystem::path(files_prefix + ".doc2rank"));
    (void)d2r_mem;
    document_table_type doc2rank_table(d2r_view);
    auto [r2d_mem, r2d_view] = irk::make_memory_view(
        boost::filesystem::path(files_prefix + ".rank2doc"));
    (void)r2d_mem;
    document_table_type rank2doc_table(r2d_view);
    return docmap(doc2rank_table.to_vector(), rank2doc_table.to_vector());
}

inline std::string ExistingDirectory(const std::string& filename)
{
    struct stat buffer{};
    bool exist = stat(filename.c_str(), &buffer) == 0;
    bool is_dir = (buffer.st_mode & S_IFDIR) != 0;  // NOLINT
    if (not exist) { return "Directory does not exist: " + filename; }
    if (not is_dir) { return "Directory is actually a file: " + filename; }
    return std::string();
}

enum class ProcessingType { TAAT, DAAT };
inline std::ostream& operator<<(std::ostream& os, ProcessingType proc_type)
{
    switch (proc_type) {
    case ProcessingType::TAAT: os << "taat"; break;
    case ProcessingType::DAAT: os << "daat"; break;
    default: throw std::domain_error("ProcessingType: non-exhaustive switch");
    }
    return os;
}

CLI::Option* add_processing_type(
    CLI::App& app,
    std::string name,
    ProcessingType& variable,
    std::string description = "",
    bool defaulted = false)
{
    CLI::callback_t fun = [&variable](CLI::results_t res) {
        if (res[0] == "taat") {
            variable = ProcessingType::TAAT;
            return true;
        }
        else if (res[0] == "daat") {
            variable = ProcessingType::DAAT;
            return true;
        }
        return false;
    };

    CLI::Option *opt = app.add_option(name, fun, description, defaulted);
    opt->type_name("ProcessingType")->type_size(1);
    if (defaulted) {
        std::stringstream out;
        out << variable;
        opt->default_str(out.str());
    }
    return opt;
}

CLI::Option* add_traversal_type(CLI::App& app,
                                std::string name,
                                Traversal_Type& variable,
                                std::string description = "",
                                bool defaulted = false)
{
    CLI::callback_t fun = [&variable](CLI::results_t res) {
        if (res[0] == "taat") {
            variable = Traversal_Type::TAAT;
            return true;
        }
        else if (res[0] == "daat") {
            variable = Traversal_Type::DAAT;
            return true;
        }
        return false;
    };

    CLI::Option *opt = app.add_option(name, fun, description, defaulted);
    opt->type_name("Traversal Type")->type_size(1);
    if (defaulted) {
        std::stringstream out;
        out << variable;
        opt->default_str(out.str());
    }
    return opt;
}

template<class Index, class RngRng>
inline auto process_query(
    const Index& index, const RngRng& postings, int k, ProcessingType type)
{
    switch (type) {
    case ProcessingType::TAAT: {
        return taat(gsl::make_span(postings), index.collection_size(), k);
    }
    case ProcessingType::DAAT: {
        return daat(gsl::make_span(postings), k);
    }
    default: throw std::domain_error("ProcessingType: non-exhaustive switch");
    }
}

enum class ThresholdEstimator { taily };
inline std::ostream& operator<<(std::ostream& os, ThresholdEstimator estimator)
{
    switch (estimator) {
        case ThresholdEstimator::taily:
            os << "taily";
            break;
        default:
            throw std::domain_error(
                "ThresholdEstimator: non-exhaustive switch");
    }
    return os;
}

CLI::Option* add_threshold_estimator(
    CLI::App& app,
    std::string name,
    std::optional<ThresholdEstimator>& variable,
    std::string description = "",
    bool defaulted = false)
{
    CLI::callback_t fun = [&variable](CLI::results_t res) {
        if (res[0] == "taily") {
            variable = ThresholdEstimator::taily;
            return true;
        }
        return false;
    };

    CLI::Option *opt = app.add_option(name, fun, description, defaulted);
    opt->type_name("ThresholdEstimator")->type_size(1);
    if (defaulted) {
        std::stringstream out;
        out << *variable;
        opt->default_str(out.str());
    }
    return opt;
}

template<class... Options>
class args : public Options... {
public:
    args(Options... opts) : Options(opts)... {}
};

struct required_tag {
} required;

struct optional_tag {
} optional;

template<typename T>
struct with_default {
    T value;
};

struct index_dir_opt {
    std::string index_dir;

    index_dir_opt(std::string default_dir = ".")
        : index_dir(std::move(default_dir))
    {}

    template<class Args>
    void set(CLI::App& app, Args& args)
    {
        app.add_option(
               "-d,--index-dir", args->index_dir, "Index directory", true)
            ->check(CLI::ExistingDirectory);
    }
};

struct reordering_opt {
    std::string reordering;

    template<class Args>
    void set(CLI::App& app, Args& args)
    {
        app.add_option("-r,--reorder",
            args->reordering,
            "Name of document reordering",
            false);
    }
};

struct metric_opt {
    std::string metric;
    const bool required;

    metric_opt(bool required = true) : required(required) {}

    template<class Args>
    void set(CLI::App& app, Args& args)
    {
        auto opt =
            app.add_option("-m,--metric", args->metric, "Metric name", false);
        if (required) { opt->required(); }
    }
};

struct etcutoff_opt {
    document_t doc_cutoff;
    double frac_cutoff;

    template<class Args>
    void set(CLI::App& app, Args& args)
    {
        auto frac_cutoff_ptr =
            app.add_option(
                   "--frac-cutoff",
                   frac_cutoff,
                   "Early termination cutoff (top fraction of collection)")
                ->check(CLI::Range(0.0, 1.0));
        auto doc_cutoff_ptr =
            app.add_option(
                   "--doc-cutoff", doc_cutoff, "Early termination docID cutoff")
                ->excludes(frac_cutoff_ptr);
        frac_cutoff_ptr->excludes(doc_cutoff_ptr);
    }
};

struct id_range_opt {
    std::vector<double> id_range = {0.0, 1.0};

    template<class Args>
    void set(CLI::App& app, Args& args)
    {
        app.add_option("--id-range", id_range, "ID range [0.0, 1.0)");
        //->check(CLI::Range(0.0, 1.0));
    }
};

struct nostem_opt {
    bool nostem = false;

    template<class Args>
    void set(CLI::App& app, Args& args)
    {
        app.add_flag("--nostem", args->nostem, "Do not stem terms");
    }
};

struct noheader_opt {
    bool noheader = false;

    template<class Args>
    void set(CLI::App& app, Args& args)
    {
        app.add_option(
            "--noheader", args->noheader, "Do not print header", true);
    }
};

struct sep_opt {
    std::string separator = "\t";

    template<class Args>
    void set(CLI::App& app, Args& args)
    {
        app.add_option("--sep", args->separator, "Field separator", true);
    }
};

struct trec_run_opt {
    std::string trec_run = "null";

    template<class Args>
    void set(CLI::App& app, Args& args)
    {
        app.add_option("--run", args->trec_run, "Trec run ID", true);
    }
};

struct trec_id_opt {
    int trec_id = -1;

    template<class Args>
    void set(CLI::App& app, Args& args)
    {
        app.add_option(
            "--trec-id",
            args->trec_id,
            "Print in trec_eval format with this QID",
            false);
    }
};

struct k_opt {
    int k = 1000;

    template<class Args>
    void set(CLI::App& app, Args& args)
    {
        app.add_option("-k", args->k, "Number of documents to retrieve", true);
    }
};

struct threads_opt {
    int threads = tbb::task_scheduler_init::default_num_threads();

    template<class Args>
    void set(CLI::App& app, Args& args)
    {
        app.add_option(
            "--threads,-j", args->threads, "Number of threads", true);
    }
};

struct traversal_type_opt {
    Traversal_Type traversal_type;

    traversal_type_opt(with_default<Traversal_Type> default_val) : traversal_type(default_val.value)
    {}

    template<class Args>
    void set(CLI::App& app, Args& args)
    {
        add_traversal_type(app, "--traversal", args->traversal_type, "Query traversal type", true);
    }
};

struct processing_type_opt {
    ProcessingType processing_type;

    processing_type_opt(with_default<ProcessingType> default_val)
        : processing_type(default_val.value)
    {}

    template<class Args>
    void set(CLI::App& app, Args& args)
    {
        add_processing_type(
            app,
            "--proctype",
            args->processing_type,
            "Query processing type",
            true);
    }
};

struct score_function_opt {
    std::string score_function;

    score_function_opt(with_default<std::string> default_val = {""})
        : score_function(default_val.value)
    {}

    template<class Args>
    void set(CLI::App& app, Args& args)
    {
        app.add_option(
            "--score", args->score_function, "Score function", false);
    }

    bool score_function_defined() const { return not score_function.empty(); }
};

struct terms_pos {
    const bool required;
    std::vector<std::string> terms;

    terms_pos(required_tag) : required(true) {}
    terms_pos(optional_tag) : required(false) {}

    template<class Args>
    void set(CLI::App& app, Args& args)
    {
        auto opt = app.add_option("terms", args->terms, "Terms", false);
        if (required) { opt->required(); }
    }
};

struct query_opt {
    std::vector<std::string> terms_or_files;
    int k;
    bool read_files;
    int trecid = -1;
    std::string trecrun = "null";

    query_opt(int default_k = 1'000) : k(default_k) {}

    template<class Args>
    void set(CLI::App& app, Args& args)
    {
        app.add_option(
               "query",
               args->terms_or_files,
               "Query terms, or query files if -f defined",
               false)
            ->required();
        app.add_option("-k", args->k, "Number of documents to retrieve", true);
        app.add_option(
            "--trecid",
            args->trecid,
            "Print in trec_eval format with this QID");
        app.add_option("--run", args->trecrun, "TREC run ID");
    }
};

template<class... Options>
inline std::pair<std::unique_ptr<CLI::App>, std::unique_ptr<args<Options...>>>
app(const std::string& description, Options... options)
{
    auto app = std::make_unique<CLI::App>(description);
    auto a = std::make_unique<args<Options...>>(options...);
    (options.set(*app, a), ...);
    return std::make_pair(std::move(app), std::move(a));
}

inline void stem_if(bool stem, std::vector<std::string>& terms)
{
    if (stem) {
        irk::porter2_stemmer stemmer;
        for (auto& term : terms) { term = stemmer.stem(term); }
    }
}

inline bool on_fly(const std::string& scorer)
{
    return scorer[0] == '*';
}

template<class Index>
inline auto postings_on_fly(
    const std::string& term, const Index& index, const std::string& name)
{
    if (name == "*bm25") {
        score::bm25_scorer scorer(
            index.term_collection_frequency(term),
            index.collection_size(),
            index.avg_document_size());
        return index.postings(term).scored(
            score::BM25TermScorer{index, std::move(scorer)});
    } else if (name == "*ql") {
        score::query_likelihood_scorer scorer(
            index.term_occurrences(term),
            index.occurrences_count(),
            index.max_document_size());
        return index.postings(term).scored(
            score::QueryLikelihoodTermScorer{index, std::move(scorer)});
    }
    else {
        throw std::domain_error(
            fmt::format("unknown score function: {}", name));
    }
}

template<class Index, class Range>
inline auto
postings_on_fly(Range& terms, const Index& index, const std::string& name)
{
    if (name == "*bm25") {
        return query_scored_postings(index, terms, irk::score::bm25);
    } else if (name == "*ql") {
        return query_scored_postings(
            index, terms, irk::score::query_likelihood);
    }
    else {
        throw std::domain_error(
            fmt::format("unknown score function: {}", name));
    }
}

struct log_finished {
    std::shared_ptr<spdlog::logger> log;
    template<class Unit>
    void operator()(const Unit& time) const
    {
        log->info("Finished in {}", irk::format_time(time));
    }
};

}  // namespace irk::cli
