#pragma once

#include <algorithm>
#include <bitset>
#include <experimental/filesystem>
#include <fstream>
#include <gsl/span>
#include <range/v3/utility/concepts.hpp>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "irkit/coding.hpp"
#include "irkit/daat.hpp"
#include "irkit/types.hpp"
#include "irkit/score.hpp"

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

    struct IndexLoadException {
        fs::path file;
        std::ios_base::failure reason;
        std::string what() {
            return "IndexLoadException: Failed to read: " + file.u8string()
                + "; reason: " + reason.what();
        }
    };

};  // namespace index

//! A posting range with scores calculated on the fly.
/*!
 * @tparam Posting  The posting structure type: must have `Posting::doc` and
 *                  `Posting::score` members.
 * @tparam Freq     An integral type of term frequencies.
 * @tparam ScoreFn  A scoring function or structure
 */
template<class Posting,
    class Freq,
    class ScoreFn = score::TfIdf,
    CONCEPT_REQUIRES_(ranges::Integral<Freq>())>
class DynamiclyScoredPostingRange {
    using Doc = doc_t<Posting>;
    using Score = score_t<Posting>;
    std::vector<Doc> docs_;
    std::vector<Freq> counts_;
    Freq term_df_;
    std::size_t n_;
    ScoreFn score_fn_;

public:
    //! Constructs a posting range for a term.
    /*!
     * @param docs      Document IDs.
     * @param counts    Occurrences of the term in respective documents.
     * @param term_df   The term's document frequency.
     * @param n         The collection size.
     * @param score_fn  The scoring function or structure of the structure:
     *                  (Freq tf, Freq df, std::size_t n) -> score_t<Posting>
     */
    DynamiclyScoredPostingRange(std::vector<Doc>&& docs,
        std::vector<Freq>&& counts,
        Freq term_df,
        std::size_t n,
        ScoreFn score_fn = score::TfIdf{})
        : docs_(docs),
          counts_(counts),
          term_df_(term_df),
          n_(n),
          score_fn_(score_fn)
    {}
    class iterator {
        typename std::vector<Doc>::const_iterator doc_iter_;
        typename std::vector<Freq>::const_iterator tf_iter_;
        Freq df_;
        std::size_t n_;
        ScoreFn score_fn_;

    public:
        iterator(typename std::vector<Doc>::const_iterator doc_iter,
            typename std::vector<Freq>::const_iterator tf_iter,
            Freq df,
            std::size_t n,
            ScoreFn score_fn)
            : doc_iter_(doc_iter),
              tf_iter_(tf_iter),
              df_(df),
              n_(n),
              score_fn_(score_fn)
        {}
        bool operator==(const iterator& rhs) const
        {
            return doc_iter_ == rhs.doc_iter_;
        }
        bool operator!=(const iterator& rhs) const
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
        auto operator*() const
        {
            Score score = score_fn_(*tf_iter_, df_, n_);
            return Posting{*doc_iter_, score};
        }
    };
    iterator cbegin() const
    {
        return iterator{
            docs_.begin(), counts_.begin(), term_df_, n_, score_fn_};
    }
    iterator cend() const
    {
        return iterator{docs_.end(), counts_.end(), term_df_, n_, score_fn_};
    }
    iterator begin() const { return cbegin(); }
    iterator end() const { return cend(); }
    std::size_t size() { return docs_.size(); }
};

template<class Doc = std::size_t,
    class Term = std::string,
    class TermId = std::size_t,
    class Freq = std::size_t>
class Index {
private:
    class TermHash {
    public:
        size_t operator()(const Term* term) const
        {
            return std::hash<std::string>()(*term);
        }
    };

    class TermEqual {
    public:
        bool operator()(const Term* lhs, const Term* rhs) const
        {
            return *lhs == *rhs;
        }
    };

    const fs::path dir_;
    bool in_memory_;
    bool skip_term_map_;
    std::vector<std::shared_ptr<Term>> terms_;
    std::vector<Freq> term_dfs_;
    std::vector<char> doc_ids_;
    std::vector<std::size_t> doc_ids_off_;
    std::vector<char> doc_counts_;
    std::vector<std::size_t> doc_counts_off_;
    std::vector<std::string> titles_;
    std::size_t doc_ids_size_;
    std::size_t doc_counts_size_;
    std::unordered_map<const Term*, TermId, TermHash, TermEqual> term_map_;

    std::optional<std::size_t> offset(const std::string& term,
        const std::vector<std::size_t>& offset_table) const
    {
        auto pos = term_map_.find(&term);
        if (pos == term_map_.end()) {
            return std::nullopt;
        }
        TermId term_id = pos->second;
        return offset(term_id, offset_table);
    }

    std::pair<std::size_t, std::size_t> locate(TermId term_id,
        const std::vector<std::size_t>& offset_table,
        std::size_t file_size) const
    {
        std::size_t offset = offset_table[term_id];
        std::size_t following = std::size_t(term_id) + 1 < offset_table.size()
            ? offset_table[term_id + 1]
            : file_size;
        return std::make_pair(offset, following - offset);
    }

    template<class T>
    std::vector<T> decode_range(TermId term_id,
        const std::vector<char>& data_container,
        const std::vector<std::size_t>& offset_table,
        bool delta) const
    {
        VarByte<T> vb;
        if (term_id >= offset_table.size()) {
            throw std::out_of_range(
                "Requested term ID out of range; requested: "
                + std::to_string(term_id) + " but must be [0, "
                + std::to_string(term_map_.size()) + ")");
        }
        auto[offset, range_size] =
            locate(term_id, offset_table, data_container.size());
        return delta ? vb.delta_decode(gsl::span<const char>(
                           data_container.data() + offset, range_size))
                     : vb.decode(gsl::span<const char>(
                           data_container.data() + offset, range_size));
    }

    std::size_t offset(
        TermId term_id, const std::vector<std::size_t>& offset_table) const
    {
        if (term_id >= term_map_.size()) {
            throw std::out_of_range(
                "Requested term ID out of range; requested: "
                + std::to_string(term_id) + " but must be [0, "
                + std::to_string(term_map_.size()) + ")");
        }
        return offset_table[term_id];
    }

    template<class ScoreFn = score::TfIdf>
    auto empty_posting_range() const -> DynamiclyScoredPostingRange<
        _Posting<Doc, score::score_result_t<ScoreFn, Doc, Freq>>,
        Freq,
        ScoreFn>
    {
        using Posting =
            _Posting<Doc, score::score_result_t<ScoreFn, Doc, Freq>>;
        return DynamiclyScoredPostingRange<Posting, Freq, ScoreFn>(
            std::vector<Doc>(), std::vector<Freq>(), Freq(0), 0, ScoreFn{});
    }

    void enforce_exist(fs::path file) const
    {
        if (!fs::exists(file)) {
            throw std::invalid_argument(
                "File not found: " + file.generic_string());
        }
    }

public:
    Index(std::vector<Term> terms,
        std::vector<Freq> term_dfs,
        std::vector<char> doc_ids,
        std::vector<std::size_t> doc_ids_off,
        std::vector<char> doc_counts,
        std::vector<std::size_t> doc_counts_off,
        std::vector<std::string> titles)
        : dir_(""),
          in_memory_(true),
          skip_term_map_(false),
          term_dfs_(std::move(term_dfs)),
          doc_ids_(std::move(doc_ids)),
          doc_ids_off_(std::move(doc_ids_off)),
          doc_counts_(std::move(doc_counts)),
          doc_counts_off_(std::move(doc_counts_off)),
          titles_(std::move(titles)),
          doc_ids_size_(doc_ids_.size()),
          doc_counts_size_(doc_counts_.size())
    {
        TermId term_id(0);
        for (const Term& term : terms) {
            terms_.push_back(std::make_shared<std::string>(std::move(term)));
            term_map_[terms_.back().get()] = term_id++;
        }
    }
    Index(fs::path dir, bool in_memory = true, bool skip_term_map = false)
        : dir_(dir), in_memory_(in_memory), skip_term_map_(skip_term_map)
    {
        load_term_map(index::terms_path(dir));
        load_term_dfs(index::term_doc_freq_path(dir));
        load_offsets(index::doc_ids_off_path(dir), doc_ids_off_);
        load_offsets(index::doc_counts_off_path(dir), doc_counts_off_);
        if (in_memory) {
            load_data(index::doc_ids_path(dir), doc_ids_);
            load_data(index::doc_counts_path(dir), doc_counts_);
        }
        load_titles(index::titles_path(dir));
        doc_ids_size_ = file_size(index::doc_ids_path(dir));
        doc_counts_size_ = file_size(index::doc_counts_path(dir));
    }
    void load_term_dfs(fs::path term_df_file)
    {
        enforce_exist(term_df_file);
        VarByte<Freq> vb;
        std::vector<char> data;
        load_data(term_df_file, data);
        term_dfs_ = vb.decode(data);
    }
    void load_titles(fs::path titles_file)
    {
        enforce_exist(titles_file);
        std::ifstream ifs(titles_file);
        std::string title;
        try {
            while (std::getline(ifs, title)) {
                titles_.push_back(title);
            }
        } catch (std::ios_base::failure& fail) {
            throw index::IndexLoadException{titles_file, fail};
        }
        ifs.close();
    }
    void load_term_map(fs::path term_file)
    {
        enforce_exist(term_file);
        std::ifstream ifs(term_file);
        std::string term;
        TermId term_id(0);
        while (std::getline(ifs, term)) {
            terms_.push_back(std::make_shared<std::string>(std::move(term)));
            if (!skip_term_map_) {
                term_map_[terms_.back().get()] = term_id++;
            }
        }
        ifs.close();
    }
    void load_offsets(fs::path offset_file, std::vector<std::size_t>& offsets)
    {
        enforce_exist(offset_file);
        VarByte<std::size_t> vb;
        std::vector<char> data;
        load_data(offset_file, data);
        offsets = vb.delta_decode(data);
    }

    void load_data(fs::path data_file, std::vector<char>& data_container) const
    {
        std::cout << "Loading data from: " << data_file << std::endl;
        enforce_exist(data_file);
        std::ifstream ifs(data_file, std::ios::binary);
        ifs.seekg(0, std::ios::end);
        std::streamsize size = ifs.tellg();
        ifs.seekg(0, std::ios::beg);
        data_container.resize(size);
        if (!ifs.read(data_container.data(), size)) {
            throw std::runtime_error("Failed reading " + data_file.u8string());
        }
        ifs.close();
    }

    std::size_t file_size(fs::path file)
    {
        enforce_exist(file);
        std::ifstream ifs(file, std::ios::binary);
        ifs.seekg(0, std::ios::end);
        std::streamsize size = ifs.tellg();
        ifs.close();
        return size;
    }

    std::vector<char>
    load_data(fs::path data_file, std::size_t start, std::size_t size) const
    {
        //std::cout << "Loading data from: " << data_file << std::endl;
        enforce_exist(data_file);
        std::ifstream ifs(data_file, std::ios::binary);
        ifs.seekg(start, std::ios::beg);
        std::vector<char> data_container;
        data_container.resize(size);
        if (!ifs.read(data_container.data(), size)) {
            throw std::runtime_error("Failed reading " + data_file.u8string());
        }
        ifs.close();
        return data_container;
    }

    std::size_t collection_size() const { return titles_.size(); }

    template<class ScoreFn = score::TfIdf>
    auto posting_ranges(const std::vector<std::string>& terms,
        ScoreFn score_fn = score::TfIdf{}) const
        -> std::vector<DynamiclyScoredPostingRange<
            _Posting<Doc, score::score_result_t<ScoreFn, Doc, Freq>>,
            Freq,
            ScoreFn>>
    {
        using Posting = _Posting<Doc, score::score_result_t<ScoreFn, Doc, Freq>>;
        std::vector<DynamiclyScoredPostingRange<Posting, Freq, ScoreFn>> ranges;
        for (const auto& term : terms) {
            ranges.push_back(posting_range(term, score_fn));
        }
        return ranges;
    }

    template<class ScoreFn = score::TfIdf>
    auto posting_range(
        const std::string& term, ScoreFn score_fn = score::TfIdf{}) const
        -> DynamiclyScoredPostingRange<
            _Posting<Doc, score::score_result_t<ScoreFn, Doc, Freq>>,
            Freq,
            ScoreFn>
    {
        auto it = term_map_.find(&term);
        if (it == term_map_.end()) {
            return empty_posting_range<ScoreFn>();
        }
        return posting_range(it->second, score_fn);
    }

    template<class ScoreFn = score::TfIdf>
    auto posting_range(TermId term_id, ScoreFn score_fn = score::TfIdf{}) const
        -> DynamiclyScoredPostingRange<
            _Posting<Doc, score::score_result_t<ScoreFn, Doc, Freq>>,
            Freq,
            ScoreFn>
    {
        using Posting = _Posting<Doc, score::score_result_t<ScoreFn, Doc, Freq>>;
        Freq df = term_dfs_[term_id];
            std::vector<Doc> docs;
            std::vector<Freq> tfs;
        if (in_memory_) {
            docs = decode_range<Doc>(term_id, doc_ids_, doc_ids_off_, true);
            tfs = decode_range<Freq>(
                term_id, doc_counts_, doc_counts_off_, false);
        } else {
            auto[doc_offset, doc_range_size] =
                locate(term_id, doc_ids_off_, doc_ids_size_);
            auto[count_offset, count_range_size] =
                locate(term_id, doc_counts_off_, doc_counts_size_);
            docs = VarByte<Doc>{}.delta_decode(load_data(
                index::doc_ids_path(dir_), doc_offset, doc_range_size));
            tfs = VarByte<Freq>{}.decode(load_data(
                index::doc_counts_path(dir_), count_offset, count_range_size));
        }
        return DynamiclyScoredPostingRange<Posting, Freq, ScoreFn>(
            std::move(docs), std::move(tfs), df, titles_.size(), score_fn);
    }

    std::string title(Doc doc_id) const { return titles_[doc_id]; }
    const std::vector<std::string>& titles() const { return titles_; }
    Term term(TermId term_id) const { return *terms_[term_id]; }
    std::size_t term_count() const { return terms_.size(); }
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

    //! Returns the number of distinct terms.
    std::size_t term_count() { return term_map_.size(); }

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
        VarByte<Doc> vb;
        //std::size_t offset = 0;
        std::size_t offset_delta = 0;
        std::vector<std::size_t> offset_deltas;
        for (auto& term : sorted_terms_.value()) {
            //off.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
            offset_deltas.push_back(offset_delta);
            TermId term_id = term_map_[term];
            Doc last = 0;
            std::vector<Doc> deltas;
            for (const auto& posting : postings_[term_id]) {
                deltas.push_back(posting.doc - last);
                last = posting.doc;
            }
            std::vector<char> encoded = vb.encode(deltas);
            out.write(encoded.data(), encoded.size());
            offset_delta = encoded.size();
        }
        VarByte<std::size_t> vboff;
        std::vector<char> encoded_offsets = vboff.encode(offset_deltas);
        off.write(encoded_offsets.data(), encoded_offsets.size());
    }

    //! Writes term-document frequencies (tf).
    void write_document_counts(std::ostream& out, std::ostream& off)
    {
        if (sorted_terms_ == std::nullopt) {
            sort_terms();
        }
        VarByte<Doc> vb;
        //std::size_t offset = 0;
        std::size_t offset_delta = 0;
        std::vector<std::size_t> offset_deltas;
        for (auto& term : sorted_terms_.value()) {
            //off.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
            offset_deltas.push_back(offset_delta);
            TermId term_id = term_map_[term];
            std::vector<char> encoded = vb.encode(postings_[term_id],
                [](const auto& posting) { return posting.freq; });
            out.write(encoded.data(), encoded.size());
            offset_delta = encoded.size();
        }
        VarByte<std::size_t> vboff;
        std::vector<char> encoded_offsets = vboff.encode(offset_deltas);
        off.write(encoded_offsets.data(), encoded_offsets.size());
    }

    //! Writes document frequencies (df).
    void write_document_frequencies(std::ostream& out)
    {
        if (sorted_terms_ == std::nullopt) {
            sort_terms();
        }
        std::vector<Freq> dfs;
        for (auto& term : sorted_terms_.value()) {
            TermId term_id = term_map_[term];
            dfs.push_back(document_frequency(term_id));
        }
        VarByte<Freq> vb;
        std::vector<char> encoded = vb.encode(dfs);
        out.write(encoded.data(), encoded.size());
    }
};

template<class Doc = std::size_t,
    class Term = std::string,
    class TermId = std::size_t,
    class Freq = std::size_t>
class IndexMerger {
private:
    using _Index = Index<Doc, Term, TermId, Freq>;
    struct Entry {
        TermId current_term_id;
        const _Index* index;
        Doc shift;
        std::string current_term() const
        {
            return index->term(current_term_id);
        }
        bool operator<(const Entry& rhs) const
        {
            return current_term() > rhs.current_term();
        }
        bool operator>(const Entry& rhs) const
        {
            return current_term() < rhs.current_term();
        }
        bool operator==(const Entry& rhs) const
        {
            return current_term() == rhs.current_term();
        }
        bool operator!=(const Entry& rhs) const
        {
            return current_term() != rhs.current_term();
        }
    };
    fs::path target_dir_;
    bool skip_unique_;
    std::vector<_Index> indices_;
    std::vector<Entry> heap_;
    std::ofstream terms_out_;
    std::ofstream doc_ids_;
    std::ofstream doc_counts_;
    std::vector<std::size_t> doc_ids_off_;
    std::vector<std::size_t> doc_counts_off_;
    std::vector<Freq> term_dfs_;
    std::size_t doc_offset_;
    std::size_t count_offset_;

    std::vector<Entry> indices_with_next_term()
    {
        std::vector<Entry> result;
        std::string next_term = heap_.front().current_term();
        while (!heap_.empty() && heap_.front().current_term() == next_term) {
            std::pop_heap(heap_.begin(), heap_.end());
            result.push_back(heap_.back());
            heap_.pop_back();
        }
        return result;
    }

public:
    IndexMerger(fs::path target_dir,
        std::vector<fs::path> indices,
        bool skip_unique = false)
        : target_dir_(target_dir), skip_unique_(skip_unique)
    {
        for (fs::path index_dir : indices) {
            std::cout << "Loading index " << index_dir << std::endl;
            indices_.emplace_back(index_dir, false, true);
        }
        terms_out_.open(index::terms_path(target_dir));
        doc_ids_.open(index::doc_ids_path(target_dir));
        doc_counts_.open(index::doc_counts_path(target_dir));
        doc_offset_ = 0;
        count_offset_ = 0;
    }

    void merge_term(std::vector<Entry>& indices)
    {
        if (skip_unique_ && indices.size() == 1) {
            auto pr = indices.front().index->posting_range(
                indices.front().current_term_id, score::Count{});
            if (pr.size() == 1) {
                std::cerr << "Skipping unique term "
                          << indices.front().current_term() << std::endl;
                return;
            }
        }
        std::cerr << "Merging term " << term_dfs_.size() << " ("
                  << indices.front().current_term() << ")" << std::endl;

        // Write the term.
        terms_out_ << indices.front().current_term() << std::endl;

        // Sort by shift, i.e., effectively document IDs.
        std::sort(indices.begin(),
            indices.end(),
            [](const Entry& lhs, const Entry& rhs) {
                return lhs.shift < rhs.shift;
            });

        // Merge the posting lists.
        std::vector<Doc> doc_ids;
        std::vector<Freq> doc_counts;
        for (const Entry& e : indices) {
            auto pr = e.index->posting_range(e.current_term_id, score::Count{});
            for (auto p : pr) {
                doc_ids.push_back(p.doc + e.shift);
                doc_counts.push_back(p.score);
            }
        }

        // Accumulate the term's document frequency.
        term_dfs_.push_back(doc_ids.size());

        // Accumulate offsets.
        doc_ids_off_.push_back(doc_offset_);
        doc_counts_off_.push_back(count_offset_);

        // Write documents and counts.
        std::vector<char> encoded_ids = VarByte<Doc>{}.delta_encode(doc_ids);
        doc_ids_.write(encoded_ids.data(), encoded_ids.size());
        std::vector<char> encoded_counts = VarByte<Freq>{}.encode(doc_counts);
        doc_counts_.write(encoded_counts.data(), encoded_counts.size());

        // Calc new offsets.
        doc_offset_ += encoded_ids.size();
        count_offset_ += encoded_counts.size();
    }

    void merge_terms()
    {
        // Initialize heap: everyone starts with the first term.
        Doc shift(0);
        for (const _Index& index : indices_) {
            heap_.push_back({0, &index, shift});
            shift += index.collection_size();
        }

        while (!heap_.empty()) {
            std::vector<Entry> indices_to_merge = indices_with_next_term();
            merge_term(indices_to_merge);
            for (const Entry& e : indices_to_merge) {
                if (std::size_t(e.current_term_id + 1)
                    < e.index->term_count()) {
                    heap_.push_back(
                        Entry{e.current_term_id + 1, e.index, e.shift});
                    std::push_heap(heap_.begin(), heap_.end());
                }
            }
        }

        // Write offsets
        VarByte<std::size_t> vb;
        std::vector<char> encoded_id_offsets = vb.delta_encode(doc_ids_off_);
        std::vector<char> encoded_count_offsets =
            vb.delta_encode(doc_counts_off_);
        std::ofstream idoff(index::doc_ids_off_path(target_dir_));
        idoff.write(encoded_id_offsets.data(), encoded_id_offsets.size());
        idoff.close();
        std::ofstream countoff(index::doc_counts_off_path(target_dir_));
        countoff.write(
            encoded_count_offsets.data(), encoded_count_offsets.size());
        countoff.close();

        // Write term dfs.
        std::vector<char> encoded_dfs = VarByte<Freq>{}.delta_encode(term_dfs_);
        std::ofstream dfs(index::term_doc_freq_path(target_dir_));
        dfs.write(encoded_dfs.data(), encoded_dfs.size());
        dfs.close();

        // Close other streams.
        terms_out_.close();
        doc_ids_.close();
        doc_counts_.close();
    }

    void merge_titles()
    {
        std::ofstream tout(index::titles_path(target_dir_));
        for (const auto& index : indices_) {
            for (const auto& title : index.titles()) {
                tout << title << std::endl;
            }
        }
        tout.close();
    }
};

using DefaultIndexBuilder =
    IndexBuilder<std::uint32_t, std::string, std::uint32_t, std::uint32_t>;
using DefaultIndexMerger =
    IndexMerger<std::uint32_t, std::string, std::uint32_t, std::uint32_t>;
using DefaultIndex =
    Index<std::uint32_t, std::string, std::uint32_t, std::uint32_t>;

};  // namespace irkit
