#pragma once

#include <assert.h>
#include <experimental/filesystem>
#include <fstream>
#include <gsl/span>
#include <iostream>
#include <type_safe/strong_typedef.hpp>
#include <unordered_map>
#include "json.hpp"

namespace bloodhound {

namespace fs = std::experimental::filesystem;

namespace json = nlohmann;

struct TermId : type_safe::strong_typedef<TermId, uint64_t>,
                type_safe::strong_typedef_op::equality_comparison<TermId>,
                type_safe::strong_typedef_op::relational_comparison<TermId>,
                type_safe::strong_typedef_op::input_operator<TermId>,
                type_safe::strong_typedef_op::output_operator<TermId> {
    using strong_typedef::strong_typedef;
};

struct Offset : type_safe::strong_typedef<Offset, uint64_t>,
                type_safe::strong_typedef_op::equality_comparison<Offset>,
                type_safe::strong_typedef_op::relational_comparison<Offset>,
                type_safe::strong_typedef_op::input_operator<Offset>,
                type_safe::strong_typedef_op::output_operator<Offset> {
    using strong_typedef::strong_typedef;
    friend const char* operator+(const char* ptr, Offset offset)
    {
        return ptr + get(offset);
    }
    friend char* operator+(char* ptr, Offset offset)
    {
        return ptr + get(offset);
    }
};

struct Score : type_safe::strong_typedef<Score, uint32_t>,
               type_safe::strong_typedef_op::integer_arithmetic<Score>,
               type_safe::strong_typedef_op::equality_comparison<Score>,
               type_safe::strong_typedef_op::relational_comparison<Score>,
               type_safe::strong_typedef_op::input_operator<Score>,
               type_safe::strong_typedef_op::output_operator<Score>,
               type_safe::strong_typedef_op::bitmask<Score> {
    using strong_typedef::strong_typedef;
};

struct Doc : type_safe::strong_typedef<Doc, uint32_t>,
             type_safe::strong_typedef_op::integer_arithmetic<Doc>,
             type_safe::strong_typedef_op::equality_comparison<Doc>,
             type_safe::strong_typedef_op::relational_comparison<Doc>,
             type_safe::strong_typedef_op::input_operator<Doc>,
             type_safe::strong_typedef_op::output_operator<Doc> {
    using strong_typedef::strong_typedef;
    operator std::size_t() { return static_cast<std::size_t>(get(*this)); }
};

using AccumulatorArray = std::vector<Score>;
using Lexicon = std::unordered_map<TermId, Offset>;
using MaxScores = std::unordered_map<TermId, Score>;

struct Posting {
    Doc doc;
    Score score;
};

struct TermWeight {
    TermId term;
    Score weight;
};

class PostingList {

public:
    struct iterator {
        const PostingList* posting_list;
        std::size_t pos;
        Posting current;

        iterator(const PostingList* pl, std::size_t p)
            : posting_list(pl), pos(p)
        {}
        Posting& operator*()
        {
            current = {posting_list->docs[pos], posting_list->scores[pos]};
            return current;
        }
        Posting* operator->()
        {
            current = {posting_list->docs[pos], posting_list->scores[pos]};
            return &current;
        }
        iterator& operator++()
        {
            ++pos;
            return *this;
        }
        iterator operator+(int n) const { return {posting_list, pos + n}; }
        iterator operator-(int n) const
        {
            return {posting_list, pos >= 3 ? pos - n : 0};
        }
        bool operator==(iterator rhs) const
        {
            return (pos == rhs.pos) && (posting_list == rhs.posting_list);
        }
        bool operator!=(iterator rhs) const
        {
            return (pos != rhs.pos) || (posting_list != rhs.posting_list);
        }
    };

    gsl::span<Doc> docs;
    gsl::span<Score> scores;
    std::optional<Score> max_score;
    std::size_t idx;
    std::size_t end_idx;

    PostingList(Doc* d, Score* s, uint32_t l)
        : docs(d, l), scores(s, l), max_score(std::nullopt), idx(0), end_idx(l)
    {}
    PostingList(Doc* d, Score* s, uint32_t l, Score ms)
        : docs(d, l), scores(s, l), max_score(ms), idx(0), end_idx(l)
    {}

    std::size_t length() const { return doc_end() - doc_begin(); }
    bool empty() const { return docs.size() == 0; }
    iterator next_ge(iterator current, Doc doc) const
    {
        while (current != end() && current->doc < doc) {
            ++current;
        }
        return current;
    }
    virtual iterator begin() const { return {this, 0}; }
    virtual iterator end() const { return {this, end_idx}; }
    virtual gsl::span<Doc>::const_iterator doc_begin() const
    {
        return docs.cbegin();
    }
    virtual gsl::span<Doc>::const_iterator doc_end() const
    {
        return docs.cbegin() + end_idx;
    }
    virtual gsl::span<Score>::const_iterator score_begin() const
    {
        return scores.cbegin();
    }
    virtual gsl::span<Score>::const_iterator score_end() const
    {
        return scores.cbegin() + end_idx;
    }
    void make_et(double et_threshold)
    {
        if (et_threshold > 0 && et_threshold <= 1.0) {
            end_idx = ceil(length() * et_threshold);
        } else {
            throw std::invalid_argument("et_threshold must be in (0,1] but is: "
                + std::to_string(et_threshold));
        }
    }
    Doc* docs_ptr() const { return &docs[0]; }
    Score* scores_ptr() const { return &scores[0]; }
};

}  // namespace bloodhound

namespace std {
template<>
struct hash<bloodhound::TermId> {
    std::size_t operator()(bloodhound::TermId const& t) const noexcept
    {
        return std::hash<std::uint64_t>{}(type_safe::get(t));
    }
};
template<>
struct hash<bloodhound::Doc> {
    std::size_t operator()(bloodhound::Doc const& t) const noexcept
    {
        return std::hash<std::uint32_t>{}(type_safe::get(t));
    }
};
}  // namespace std

namespace bloodhound::index {

std::vector<char> read_file(fs::path filepath)
{
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) {
        // TODO: throw exception
    }
    return buffer;
}

struct PostingListHeader {
    uint32_t mask;
    uint32_t doc_count;
    uint32_t position_count;
    uint32_t payload_offset;
    uint32_t position_offset;
    uint32_t section_offset;
    PostingListHeader() {}
    PostingListHeader(uint32_t m,
        uint32_t d,
        uint32_t pc,
        uint32_t pyo,
        uint32_t poo,
        uint32_t so)
        : mask(m),
          doc_count(d),
          position_count(pc),
          payload_offset(pyo),
          position_offset(poo),
          section_offset(so)
    {}
    bool checkmask(int b) const { return (mask & (1 << b)); }
    void setmask(int b) { mask |= (1 << b); }
    bool is_short() const { return checkmask(28); }
};

class Index {

private:
    std::size_t collection_size;
    std::vector<char> postings_data;
    Lexicon lexicon;
    MaxScores max_scores;

public:
    std::size_t get_collection_size() const { return collection_size; }

    PostingList posting_list(TermId termid)
    {
        const auto term_it = lexicon.find(termid);
        if (term_it == lexicon.end()) {
            return PostingList(0, 0, 0);
        }
        Score max_score = max_scores[term_it->first];
        Offset offset = term_it->second;
        auto header =
            reinterpret_cast<PostingListHeader*>(postings_data.data() + offset);
        if (header->is_short()) {
            return PostingList(reinterpret_cast<Doc*>(&header->doc_count),
                reinterpret_cast<Score*>(&header->payload_offset),
                1,
                max_score);
        }
        if (header->payload_offset == 0) {
            return PostingList(0, 0, 0);
        }
        auto count = header->doc_count;
        auto docs_ptr = reinterpret_cast<Doc*>(
            postings_data.data() + offset + sizeof(*header));
        auto scores_ptr = reinterpret_cast<Score*>(
            postings_data.data() + offset + header->payload_offset);
        return PostingList(docs_ptr, scores_ptr, count, max_score);
    }

    PostingList posting_list(TermId termid, double et_threshold)
    {
        auto postlist = posting_list(termid);
        postlist.make_et(et_threshold);
        return postlist;
    }

    static std::tuple<Lexicon, MaxScores> load_mappings(fs::path index_dir)
    {
        fs::path lexicon_file = index_dir / "dictionary.dat";
        fs::path maxscore_file = index_dir / "maxscore.dat";
        Lexicon lexicon;
        MaxScores max_scores;
        std::vector<char> lexicon_buf = read_file(lexicon_file);
        std::vector<char> max_score_buf = read_file(maxscore_file);
        std::size_t term_count = (lexicon_buf.size() - 24) / 16;
        struct lex_entry_t {
            TermId termid;
            Offset offset;
        };
        auto span_beg = reinterpret_cast<lex_entry_t*>(lexicon_buf.data() + 24);
        auto entries = gsl::span<lex_entry_t>(span_beg, term_count);
        Score* max_score = reinterpret_cast<Score*>(max_score_buf.data());
        for (auto[termid, offset] : entries) {
            lexicon[termid] = offset;
            max_scores[termid] = *(max_score++);
        }
        return std::make_tuple(lexicon, max_scores);
    }

    static json::json load_meta(fs::path meta_file)
    {
        std::ifstream meta_stream(meta_file);
        json::json meta;
        meta_stream >> meta;
        return meta;
    }

    static Index load_index(fs::path index_dir)
    {
        Index index;

        fs::path meta_file = index_dir / "manifest.json";
        auto meta = load_meta(meta_file);
        index.collection_size = meta["collection_size"];

        fs::path postings_file = index_dir / "postings.dat";
        auto[lexicon, max_scores] = load_mappings(index_dir);
        index.lexicon = std::move(lexicon);
        index.max_scores = std::move(max_scores);
        index.postings_data = read_file(postings_file);
        return index;
    }

    friend Index build_index_from_ids(
        const std::vector<std::vector<TermWeight>>& input);
    friend Index sorted_index(const Index& index);
};

/// Converts a vector of terms to a vector of posting lists
std::vector<PostingList> terms_to_postings(
    Index& index, const std::vector<TermId> terms)
{
    std::vector<PostingList> postings;
    for (auto& term : terms) {
        postings.push_back(index.posting_list(term));
    }
    return postings;
}

/// Builds an index based on a collection represented by term IDs.
///
/// This function is not efficient and only meant for testing other
/// functionalities.
Index build_index_from_ids(const std::vector<std::vector<TermWeight>>& input)
{
    Index index;
    std::vector<char>& idx_postings = index.postings_data;
    std::size_t collection_size = 0;
    std::map<TermId, std::vector<Posting>> term2doc;

    for (auto& doc_terms : input) {
        Doc doc = Doc(collection_size);
        for (auto & [term, weight] : doc_terms) {
            term2doc[term].push_back({doc, weight});
        }
        ++collection_size;
    }

    // Write postings.
    for (auto & [term, postings] : term2doc) {
        index.lexicon[term] = Offset(idx_postings.size());
        std::map<Doc, std::size_t> occurrences;
        uint32_t doc_count = postings.size();
        uint32_t payload_offset =
            sizeof(PostingListHeader) + doc_count * sizeof(doc_count);
        PostingListHeader header = {0, doc_count, 0, payload_offset, 0, 0};

        // Write header
        char* data = reinterpret_cast<char*>(&header);
        idx_postings.insert(idx_postings.end(), data, data + sizeof(header));

        // Write docs
        for (auto& posting : postings) {
            data = reinterpret_cast<char*>(&posting.doc);
            idx_postings.insert(
                idx_postings.end(), data, data + sizeof(posting.doc));
        }

        // Write scores
        Score max_score = Score(0);
        for (auto& posting : postings) {
            Score score = posting.score;
            max_score = std::max(max_score, score);
            data = reinterpret_cast<char*>(&score);
            idx_postings.insert(idx_postings.end(), data, data + sizeof(score));
        }
        index.max_scores[term] = max_score;
    }

    index.collection_size = collection_size;

    return index;
}

Index sorted_index(const Index& index)
{
    Index sorted;
    sorted.lexicon = Lexicon(index.lexicon);
    sorted.max_scores = MaxScores(index.max_scores);
    sorted.postings_data = std::vector<char>(index.postings_data);
    for (auto & [term, offset] : sorted.lexicon) {
        PostingList posting_list = sorted.posting_list(term);
        std::vector<Posting> postings;
        for (auto& posting : posting_list) {
            postings.push_back(posting);
        }
        std::sort(postings.begin(),
            postings.end(),
            [](const Posting& a, const Posting& b) {
                return a.score > b.score;
            });
        for (std::size_t posting_idx = 0; posting_idx < postings.size();
             posting_idx++) {
            Doc doc = postings[posting_idx].doc;
            Score score = postings[posting_idx].score;
            PostingListHeader* header = reinterpret_cast<PostingListHeader*>(
                sorted.postings_data.data() + offset);
            Doc* doc_sink =
                reinterpret_cast<Doc*>(sorted.postings_data.data() + offset
                    + sizeof(PostingListHeader) + posting_idx * sizeof(Doc));
            Score* score_sink =
                reinterpret_cast<Score*>(sorted.postings_data.data() + offset
                    + header->payload_offset + posting_idx * sizeof(Doc));
            *doc_sink = doc;
            *score_sink = score;
        }
    }
    return sorted;
}

}  // namespace bloodhound::index
