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

#include <CLI/CLI.hpp>
#include <CLI/Option.hpp>
#include <boost/filesystem.hpp>

#include <irkit/compacttable.hpp>
#include <irkit/index.hpp>
#include <irkit/index/source.hpp>
#include <irkit/parsing/stemmer.hpp>
#include <irkit/taat.hpp>

#include <cpprest/json.h>

namespace irk::tool {

using std::uint32_t;
using irk::index::document_t;

inline std::string ExistingDirectory(const std::string& filename)
{
    struct stat buffer{};
    bool exist = stat(filename.c_str(), &buffer) == 0;
    bool is_dir = (buffer.st_mode & S_IFDIR) != 0;  // NOLINT
    if (not exist) { return "Directory does not exist: " + filename; }
    if (not is_dir) { return "Directory is actually a file: " + filename; }
    return std::string();
}

class query {
public:
    void init(CLI::App& app, int argc, char** argv)
    {
        app.add_option("-d,--index-dir", index_dir_, "index directory", true)
            ->check(CLI::ExistingDirectory);
        app.add_option("-k", k_, "as in top-k", true);
        app.add_flag("-s,--stem", stem_, "Stem terems (Porter2)");
        app.add_flag("-f,--file", from_file_, "Read queries from file(s)");
        app.add_option(
            "--trecid", trecid_, "Print in trec_eval format with this QID");
        auto remap = app.add_option(
            "--remap", remap_name_, "Name of remapping used for cutoff");
        auto fraccut =
            app.add_option("--frac-cutoff",
                   frac_cutoff_,
                   "Early termination cutoff (top fraction of collection)")
                ->needs(remap)
                ->check(CLI::Range(0.0, 1.0));
        auto idcut = app.add_option("--doc-cutoff",
                            doc_cutoff_,
                            "Early termination docID cutoff")
                         ->needs(remap)
                         ->excludes(fraccut);
        fraccut->excludes(idcut);
        app.add_option("query", query_, "Query (or query file with -f)", false)
            ->required();
        app.parse(argc, argv);
    }

    void init(const web::json::value& config)
    {
        try {
            index_dir_ = config.at(U("index-dir")).as_string();
        } catch (const web::json::json_exception& err) {
            auto msg = std::string(err.what()) + ": index-dir";
            throw web::json::json_exception(msg.c_str());
        }
        try {
            std::string q = config.at(U("query")).as_string();
            std::istringstream in(q);
            std::string term;
            while (in >> term) { query_.push_back(std::move(term)); }
        } catch (const web::json::json_exception& err) {
            auto msg = std::string(err.what()) + ": query";
            throw web::json::json_exception(msg.c_str());
        }
        if (config.has_field("k")) { k_ = config.at("k").as_integer(); }
        if (config.has_field("stem")) { stem_ = config.at("stem").as_bool(); }
        if (config.has_field("trecid")) {
            trecid_ = config.at("trecid").as_integer();
        }
        if (config.has_field("remap")) {
            remap_name_ = config.at("remap").as_string();
        }
        if (config.has_field("frac-cutoff")) {
            frac_cutoff_ = config.at("frac-cutoff").as_double();
        }
        if (config.has_field("doc-cutoff")) {
            doc_cutoff_ = config.at("doc-cutoff").as_integer();
        }
    }

    int execute(std::ostream& out = std::cout)
    {
        boost::filesystem::path dir(index_dir_);
        irk::inverted_index_mapped_data_source data(dir, "bm25");
        irk::inverted_index_view index(&data);

        if (frac_cutoff_ < 1.0)
        {
            doc_cutoff_ = static_cast<document_t>(
                frac_cutoff_ * index.titles().size());
        }

        if (not from_file_)
        {
            run_query(out,
                index,
                dir,
                query_,
                k_,
                stem_,
                remap_name_,
                doc_cutoff_ < std::numeric_limits<document_t>::max()
                    ? std::make_optional(doc_cutoff_)
                    : std::nullopt,
                trecid_ > -1 ? std::make_optional(trecid_) : std::nullopt);
        } else
        {
            std::optional<int> current_trecid = trecid_ > -1
                ? std::make_optional(trecid_)
                : std::nullopt;
            for (const auto& file : query_)
            {
                std::ifstream in(file);
                std::string q;
                while (std::getline(in, q))
                {
                    std::istringstream qin(q);
                    std::string term;
                    std::vector<std::string> terms;
                    while (qin >> term) { terms.push_back(std::move(term)); }
                    run_query(out,
                        index,
                        dir,
                        terms,
                        k_,
                        stem_,
                        remap_name_,
                        doc_cutoff_ < std::numeric_limits<document_t>::max()
                            ? std::make_optional(doc_cutoff_)
                            : std::nullopt,
                        current_trecid);
                    if (current_trecid.has_value()) {
                        current_trecid.value()++;
                    }
                }
            }
        }
        return 0;
    }

private:
    template<class Iter, class Table>
    void
    prune(Iter begin, Iter end, const Table& doc2rank, document_t doc_cutoff)
    {
        document_t doc = 0;
        for (Iter it = begin; it != end; ++it)
        {
            if (doc2rank[doc++] > doc_cutoff) { *it = 0; }
        }
    }

    template<class Index>
    void run_query(std::ostream& out,
        const Index& index,
        const boost::filesystem::path& dir,
        std::vector<std::string>& query,
        int k,
        bool stem,
        const std::string& remap_name,
        std::optional<document_t> cutoff,
        std::optional<int> trecid)
    {
        if (stem)
        {
            irk::porter2_stemmer stemmer;
            for (auto& term : query) { term = stemmer.stem(term); }
        }

        auto start_time = std::chrono::steady_clock::now();

        auto postings = irk::query_postings(index, query);
        auto after_fetch = std::chrono::steady_clock::now();

        std::vector<uint32_t> acc(index.collection_size(), 0);
        auto after_init = std::chrono::steady_clock::now();

        irk::taat(postings, acc);
        auto after_acc = std::chrono::steady_clock::now();

        if (cutoff.has_value())
        {
            auto doc2rank = irk::load_compact_table<document_t>(
                dir / (remap_name + ".doc2rank"));
            prune(std::begin(acc), std::end(acc), doc2rank, cutoff.value());
        }

        auto results = irk::aggregate_top_k<document_t, uint32_t>(acc, k);
        auto end_time = std::chrono::steady_clock::now();

        auto total = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        auto fetch = std::chrono::duration_cast<std::chrono::milliseconds>(
            after_fetch - start_time);
        auto init = std::chrono::duration_cast<std::chrono::milliseconds>(
            after_init - after_fetch);
        auto accum = std::chrono::duration_cast<std::chrono::milliseconds>(
            after_acc - after_init);
        auto agg = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - after_acc);

        const auto& titles = index.titles();
        int rank = 0;
        for (auto& result : results)
        {
            if (trecid.has_value())
            {
                out << *trecid << '\t' << "Q0\t"
                          << titles.key_at(result.first) << "\t" << rank++
                          << "\t" << result.second << "\tnull\n";
            } else
            {
                out << titles.key_at(result.first) << "\t"
                          << result.second << '\n';
            }
        }
        //std::cerr << "Time: " << total.count() << " ms [ ";
        //std::cerr << "Fetch: " << fetch.count() << " / ";
        //std::cerr << "Init: " << init.count() << " / ";
        //std::cerr << "Acc: " << accum.count() << " / ";
        //std::cerr << "Agg: " << agg.count() << " ]\n";
    }

    std::string index_dir_ = ".";
    int k_ = 1000;
    bool stem_ = false;
    bool from_file_ = false;
    int trecid_ = -1;
    std::string remap_name_{};
    double frac_cutoff_ = 1.0;
    document_t doc_cutoff_ = std::numeric_limits<document_t>::max();
    std::vector<std::string> query_{};
};

};  // namespace irk::tool
