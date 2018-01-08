#include <algorithm>
#include <cmath>
#include <experimental/filesystem>
#include <fstream>
#include <gsl/span>
#include <range/v3/utility/concepts.hpp>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "irkit/types.hpp"

namespace irkit {

namespace fs = std::experimental::filesystem;

namespace index {

    fs::path doc_ids_path(fs::path dir) { return dir / "doc.id"; };
    fs::path doc_ids_off_path(fs::path dir) { return dir / "doc.idoff"; };
    fs::path doc_counts_path(fs::path dir) { return dir / "doc.count"; };
    fs::path doc_counts_off_path(fs::path dir) { return dir / "doc.countoff"; };
    fs::path terms_path(fs::path dir) { return dir / "terms.txt"; };
    fs::path term_doc_freq_path(fs::path dir) { return dir / "terms.docfreq"; };
    fs::path titles_path(fs::path dir) { return dir / "titles.txt"; };

};  // namespace index

namespace score {

    //! Calculates a simple tf-idf score.
    /*!
     * @tparam Freq An integral type.
     * @param tf    Term frequency in the document being scored.
     * @param df    The term's document frequency in the collection, i.e., how
     *              many documents include the term.
     */
    template<class Freq, CONCEPT_REQUIRES_(ranges::Integral<Freq>())>
    double tfidf(Freq tf, Freq df, std::size_t N)
    {
        return static_cast<double>(tf)
            * std::log(N / (1 + static_cast<double>(df)));
    }

};  // namespace score

//! A posting range with scores calculated on the fly.
/*!
 * @tparam Posting  The posting structure type: must have `Posting::doc` and
 *                  `Posting::score` members.
 * @tparam Freq     An integral type of term frequencies.
 */
template<class Posting, class Freq, CONCEPT_REQUIRES_(ranges::Integral<Freq>())>
class DynamiclyScoredPostingRange {
    using Doc = doc_t<Posting>;
    using Score = score_t<Posting>;
    gsl::span<Doc> docs_;
    gsl::span<Freq> counts_;
    Freq term_df_;
    std::size_t n_;

public:
    //! Constructs a posting range for a term.
    /*!
     * @param docs      Document IDs.
     * @param counts    Occurrences of the term in respective documents.
     * @param term_df   The term's document frequency.
     * @param term_df   The collection size.
     */
    DynamiclyScoredPostingRange(gsl::span<Doc> docs,
        gsl::span<Freq> counts,
        Freq term_df,
        std::size_t n)
        : docs_(docs), counts_(counts), term_df_(term_df), n_(n)
    {}
    class iterator {
        typename gsl::span<Doc>::const_iterator doc_iter_;
        typename gsl::span<Freq>::const_iterator tf_iter_;
        Freq df_;
        std::size_t n_;

    public:
        iterator(typename gsl::span<Doc>::const_iterator doc_iter,
            typename gsl::span<Freq>::const_iterator tf_iter,
            Freq df,
            std::size_t n)
            : doc_iter_(doc_iter), tf_iter_(tf_iter), df_(df), n_(n)
        {}
        bool operator==(const iterator& rhs)
        {
            return doc_iter_ == rhs.doc_iter_;
        }
        bool operator!=(const iterator& rhs)
        {
            return doc_iter_ != rhs.doc_iter_;
        }
        void operator++()
        {
            doc_iter_++;
            tf_iter_++;
        }
        void operator++(int)
        {
            doc_iter_++;
            tf_iter_++;
        }
        auto operator*()
        {
            Score score = score::tfidf(*tf_iter_, df_, n_);
            return Posting{*doc_iter_, score};
        }
    };
    iterator begin() const
    {
        return iterator{docs_.begin(), counts_.begin(), term_df_, n_};
    }
    iterator end() const
    {
        return iterator{docs_.end(), counts_.end(), term_df_, n_};
    }
    std::size_t size() { return docs_.size(); }
};

template<class Doc = std::size_t,
    class Term = std::string,
    class TermId = std::size_t,
    class Freq = std::size_t>
class Index {
    using Posting = _Posting<Doc, double>;
    std::unordered_map<Term, TermId> term_map_;
    std::vector<Freq> term_dfs_;
    std::vector<char> doc_ids_;
    std::vector<std::size_t> doc_ids_off_;
    std::vector<char> doc_counts_;
    std::vector<std::size_t> doc_counts_off_;
    std::vector<std::string> titles_;

public:
    Index(fs::path dir)
    {
        load_term_map(index::terms_path(dir));
        load_term_dfs(index::term_doc_freq_path(dir));
        load_offsets(index::doc_ids_off_path(dir), doc_ids_off_);
        load_offsets(index::doc_counts_off_path(dir), doc_counts_off_);
        load_data(index::doc_ids_path(dir), doc_ids_);
        load_data(index::doc_counts_path(dir), doc_counts_);
        load_titles(index::titles_path(dir));
    }
    void load_term_dfs(fs::path term_df_file)
    {
        std::ifstream ifs(term_df_file, std::ios::binary);
        Freq freq;
        while (ifs.read(reinterpret_cast<char*>(&freq), sizeof(freq))) {
            term_dfs_.push_back(freq);
        }
        ifs.close();
    }
    void load_titles(fs::path titles_file)
    {
        std::ifstream ifs(titles_file);
        std::string title;
        while (std::getline(ifs, title)) {
            titles_.push_back(title);
        }
        ifs.close();
    }
    void load_term_map(fs::path term_file)
    {
        std::ifstream ifs(term_file);
        std::string term;
        TermId term_id(0);
        while (std::getline(ifs, term)) {
            term_map_[term] = term_id++;
        }
        ifs.close();
    }
    void load_offsets(fs::path offset_file, std::vector<std::size_t>& offsets)
    {
        std::ifstream ifs(offset_file, std::ios::binary);
        std::size_t offset;
        while (ifs.read(reinterpret_cast<char*>(&offset), sizeof(offset))) {
            offsets.push_back(offset);
        }
        ifs.close();
    }
    void load_data(fs::path data_file, std::vector<char>& data_container)
    {
        std::ifstream ifs(data_file, std::ios::binary);
        std::streamsize size = ifs.tellg();
        ifs.seekg(0, std::ios::beg);
        data_container.resize(size);
        if (!ifs.read(data_container.data(), size)) {
            /* TODO: Failed */
        }
        ifs.close();
    }

    std::size_t collection_size() const { return titles_.size(); }

    std::vector<DynamiclyScoredPostingRange<Posting, Freq>> posting_ranges(
        const std::vector<std::string>& terms)
    {
        std::vector<DynamiclyScoredPostingRange<Posting, Freq>> ranges;
        for (const auto& term : terms) {
            ranges.push_back(posting_range(term));
        }
        return ranges;
    }
    DynamiclyScoredPostingRange<Posting, Freq> posting_range(
        const std::string& term)
    {
        auto it = term_map_.find(term);
        if (it == term_map_.end()) {
            return DynamiclyScoredPostingRange<Posting, Freq>(
                gsl::span<Doc>(), gsl::span<Freq>(), Freq(0), 0);
        }
        return posting_range(it->second);
    }
    DynamiclyScoredPostingRange<Posting, Freq> posting_range(TermId term_id)
    {
        if (term_id >= term_map_.size()) {
            return DynamiclyScoredPostingRange<Posting, Freq>(
                gsl::span<Doc>(), gsl::span<Freq>(), Freq(0), 0);
        }
        Freq df = term_dfs_[term_id];
        gsl::span<Doc> docs(
            reinterpret_cast<Doc*>(&doc_ids_[doc_ids_off_[term_id]]), df);
        gsl::span<Freq> tfs(
            reinterpret_cast<Freq*>(&doc_counts_[doc_counts_off_[term_id]]),
            df);
        return DynamiclyScoredPostingRange<Posting, Freq>(
            docs, tfs, df, titles_.size());
    }
};

template<class Doc = std::size_t,
    class Term = std::string,
    class TermId = std::size_t,
    class Freq = std::size_t>
class IndexBuilder {
    struct DocFreq {
        Doc doc;
        Freq freq;
        bool operator==(const DocFreq& rhs) const
        {
            return doc == rhs.doc && freq == rhs.freq;
        }
    };

protected:
    Doc current_doc_;
    std::optional<std::vector<Term>> sorted_terms_;
    std::vector<std::vector<DocFreq>> postings_;
    std::vector<std::size_t> term_occurrences_;
    std::unordered_map<Term, TermId> term_map_;

public:
    IndexBuilder() : current_doc_(0) {}

    //! Initiates a new document with an incremented ID.
    void add_document() { current_doc_++; }

    //! Initiates a new document with the given ID.
    void add_document(Doc doc) { current_doc_ = doc; }

    //! Adds a term to the current document.
    void add_term(const Term& term)
    {
        auto ti = term_map_.find(term);
        if (ti != term_map_.end()) {
            TermId term_id = ti->second;
            if (postings_[term_id].back().doc == current_doc_) {
                postings_[term_id].back().freq++;
            } else {
                postings_[term_id].push_back({current_doc_, 1});
            }
            term_occurrences_[term_id]++;
        } else {
            TermId term_id = term_map_.size();
            term_map_[term] = term_id;
            postings_.push_back({{current_doc_, 1}});
            term_occurrences_.push_back(1);
        }
    }

    //! Returns the document frequency of the given term.
    Freq document_frequency(TermId term_id)
    {
        return postings_[term_id].size();
    }

    //! Returns the number of the accumulated documents.
    std::size_t collection_size() { return current_doc_ + 1; }

    //! Sorts the terms, and all related structures, lexicographically.
    void sort_terms()
    {
        sorted_terms_ = std::vector<Term>();
        for (const auto& term_entry : term_map_) {
            const Term& term = term_entry.first;
            sorted_terms_->push_back(term);
        }
        std::sort(sorted_terms_->begin(), sorted_terms_->end());

        // Create new related structures and swap.
        std::vector<std::vector<DocFreq>> postings;
        std::vector<std::size_t> term_occurrences;
        for (const std::string& term : sorted_terms_.value()) {
            TermId term_id = term_map_[term];
            term_map_[term] = postings.size();
            postings.push_back(std::move(postings_[term_id]));
            term_occurrences.push_back(std::move(term_occurrences_[term_id]));
        }
        postings_ = std::move(postings);
        term_occurrences_ = std::move(term_occurrences);
    }

    //! Writes new-line-delimited sorted list of terms.
    void write_terms(std::ostream& out)
    {
        if (sorted_terms_ == std::nullopt) {
            sort_terms();
        }
        for (auto& term : sorted_terms_.value()) {
            out << term << std::endl;
        }
    }

    //! Writes document IDs
    void write_document_ids(std::ostream& out, std::ostream& off)
    {
        if (sorted_terms_ == std::nullopt) {
            sort_terms();
        }
        std::size_t offset = 0;
        for (auto& term : sorted_terms_.value()) {
            off.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
            TermId term_id = term_map_[term];
            for (const auto& posting : postings_[term_id]) {
                auto size = sizeof(posting.doc);
                out.write(reinterpret_cast<const char*>(&posting.doc), size);
                offset += size;
            }
        }
    }

    //! Writes term-document frequencies (tf).
    void write_document_counts(std::ostream& out, std::ostream& off)
    {
        if (sorted_terms_ == std::nullopt) {
            sort_terms();
        }
        std::size_t offset = 0;
        for (auto& term : sorted_terms_.value()) {
            off.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
            TermId term_id = term_map_[term];
            for (auto& posting : postings_[term_id]) {
                auto size = sizeof(posting.freq);
                out.write(reinterpret_cast<const char*>(&posting.freq), size);
                offset += size;
            }
        }
    }

    //! Writes document frequencies (df).
    void write_document_frequencies(std::ostream& out)
    {
        if (sorted_terms_ == std::nullopt) {
            sort_terms();
        }
        for (auto& term : sorted_terms_.value()) {
            TermId term_id = term_map_[term];
            const auto df = document_frequency(term_id);
            out.write(reinterpret_cast<const char*>(&df), sizeof(df));
        }
    }
};

using DefaultIndexBuilder =
    IndexBuilder<std::uint32_t, std::string, std::uint32_t, std::uint32_t>;
using DefaultIndex =
    Index<std::uint32_t, std::string, std::uint32_t, std::uint32_t>;

};  // namespace irkit
