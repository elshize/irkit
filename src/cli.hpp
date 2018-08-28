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

#include <vector>

#include <CLI/CLI.hpp>

#include <irkit/compacttable.hpp>
#include <irkit/index/types.hpp>
#include <irkit/memoryview.hpp>
#include <irkit/parsing/stemmer.hpp>

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
    auto d2r_mem = irk::make_memory_view(
        boost::filesystem::path(files_prefix + ".doc2rank"));
    document_table_type doc2rank_table(d2r_mem);
    auto r2d_mem = irk::make_memory_view(
        boost::filesystem::path(files_prefix + ".rank2doc"));
    document_table_type rank2doc_table(r2d_mem);
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

template<class... Options>
class args : public Options... {
public:
    args(Options... opts) : Options(opts)... {}
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
        auto opt = app.add_option(
            "-m,--metric", args->metric, "Metric name", false);
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
            app.add_option("--frac-cutoff",
                   frac_cutoff,
                   "Early termination cutoff (top fraction of collection)")
                ->check(CLI::Range(0.0, 1.0));
        auto doc_cutoff_ptr = app.add_option("--doc-cutoff",
                                     doc_cutoff,
                                     "Early termination docID cutoff")
                                  ->excludes(frac_cutoff_ptr);
        frac_cutoff_ptr->excludes(doc_cutoff_ptr);
    }
};

struct query_opt {
    std::vector<std::string> terms_or_files;
    int k;
    bool nostem;
    bool read_files;
    int trecid = -1;
    std::string trecrun = "null";

    query_opt(int default_k = 1'000) : k(default_k) {}

    template<class Args>
    void set(CLI::App& app, Args& args)
    {
        app.add_option("query (files)",
               args->terms_or_files,
               "Query terms, or query files if -f defined",
               false)
            ->required();
        app.add_option("-k", args->k, "Number of documents to retrieve", true);
        app.add_flag("--nostem", args->nostem, "Skip stemming terms (Porter2)");
        app.add_flag(
            "-f,--file", args->read_files, "Read queries from file(s)");
        app.add_option("--trecid",
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

}  // namespace irk::cli
