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

#include <iostream>
#include <string>

#include <boost/te.hpp>
#include <fmt/format.h>
#include <gsl/span>
#include <irkit/algorithm/query.hpp>
#include <irkit/parsing/stemmer.hpp>
#include <irkit/score.hpp>

namespace irk {

struct Empty_Tag {
} empty_tag;

enum class Traversal_Type { TAAT, DAAT };
inline std::ostream& operator<<(std::ostream& os, Traversal_Type type)
{
    switch (type) {
    case Traversal_Type::TAAT: os << "taat"; break;
    case Traversal_Type::DAAT: os << "daat"; break;
    default: throw std::domain_error("Traversal_Type: non-exhaustive switch");
    }
    return os;
}

struct Daat_Traveral_Tag {
} daat_traversal;

inline std::ostream& operator<<(std::ostream& os, Daat_Traveral_Tag)
{
    os << "daat";
    return os;
}

struct Taat_Traveral_Tag {
} taat_traversal;

inline std::ostream& operator<<(std::ostream& os, Taat_Traveral_Tag)
{
    os << "taat";
    return os;
}

template<class Score_Tag, class Index>
auto fetch_scorers(Index const& index, gsl::span<std::string const>& terms, Score_Tag score_tag)
{
    std::vector<decltype(index.term_scorer(0, score_tag))> scorers;
    for (const auto& term : terms) {
        if (auto term_id = index.term_id(term); term_id) {
            scorers.push_back(index.term_scorer(term_id.value(), score_tag));
        } else {
            scorers.push_back(index.term_scorer(0, score_tag));
        }
    }
    return scorers;
}

template<typename Index>
inline auto fetched_query_postings(Index const& index, gsl::span<std::string const>& query_terms)
{
    using posting_list_type = decltype(index.postings(std::declval<std::string>()).fetch());
    std::vector<posting_list_type> postings;
    postings.reserve(query_terms.size());
    for (const auto& term : query_terms) {
        postings.push_back(index.postings(term).fetch());
    }
    return postings;
}

template<typename Index>
inline auto
fetched_query_scored_postings(Index const& index, gsl::span<std::string const>& query_terms)
{
    using posting_list_type = decltype(index.scored_postings(std::declval<std::string>()).fetch());
    std::vector<posting_list_type> postings;
    postings.reserve(query_terms.size());
    for (const auto& term : query_terms) {
        postings.push_back(index.scored_postings(term).fetch());
    }
    return postings;
}

struct Printable : boost::te::poly<Printable> {
    using boost::te::poly<Printable>::poly;

    void print(std::ostream& out) const
    {
        boost::te::call([](auto const& self, auto& out) { out << self; }, *this, out);
    }
};
std::ostream& operator<<(std::ostream& os, Printable const& p)
{
    p.print(os);
    return os;
}

struct Query_Result_List {
    std::vector<std::pair<irk::index::document_t, Printable>> results;

    template<class Score>
    Query_Result_List(std::vector<std::pair<irk::index::document_t, Score>> results)
        : self_(std::make_shared<Impl<Score>>(results))
    {}

    void print(std::function<void(int, irk::index::document_t, Printable)> f) const
    {
        self_->print(f);
    }

private:
    struct Result
    {
        Result() = default;
        Result(Result const&) = default;
        Result(Result&&) noexcept = default;
        Result& operator=(Result const&) = default;
        Result& operator=(Result&&) noexcept = default;
        virtual ~Result() = default;
        virtual void print(std::function<void(int, irk::index::document_t, Printable)> f) const = 0;
    };

    template<class Score>
    class Impl : public Result {
    public:
        explicit Impl(std::vector<std::pair<irk::index::document_t, Score>> results)
            : results_(results)
        {}
        void print(std::function<void(int, irk::index::document_t, Printable)> f) const override
        {
            int rank = 0;
            for (auto& result : results_) {
                f(rank++, result.first, result.second);
            }
        }
    private:
        std::vector<std::pair<irk::index::document_t, Score>> results_;
    };

private:
    std::shared_ptr<Result> self_;
};

class Query_Engine {
public:
    template<class Index, class Score_Tag, class Traversal_Tag>
    explicit Query_Engine(Index const& index,
                          bool nostem,
                          Score_Tag score_tag,
                          Traversal_Tag traversal_tag,
                          std::optional<int> trec_id,
                          std::string run_id,
                          std::ostream& out = std::cout)
        : self_(std::make_shared<Impl<Index, Score_Tag, Traversal_Tag>>(
              index, nostem, score_tag, traversal_tag, trec_id, std::move(run_id), out))
    {}

    [[nodiscard]] Query_Result_List run_query(gsl::span<std::string const> query_terms, int k)
    {
        return self_->run_query(query_terms, k);
    }

    [[nodiscard]] static auto is_quantized(std::string const& name) -> bool
    {
        return std::find(name.begin(), name.end(), '-') != name.end();
    }

    template<typename Index>
    [[nodiscard]] static Query_Engine from(Index const& index,
                                           bool nostem,
                                           std::string const& score_function,
                                           Traversal_Type traversal_type,
                                           std::optional<int> trec_id,
                                           std::string const& run_id,
                                           std::ostream& out = std::cout)
    {
        if (is_quantized(score_function)) {
            switch (traversal_type) {
            case Traversal_Type::TAAT:
                return Query_Engine(index, nostem, empty_tag, taat_traversal, trec_id, run_id, out);
            case Traversal_Type::DAAT:
                return Query_Engine(index, nostem, empty_tag, daat_traversal, trec_id, run_id, out);
            }
            throw std::runtime_error("unknown traversal type");
        } else {
            switch (traversal_type) {
            case Traversal_Type::TAAT:
                if (score_function == "bm25") {
                    return Query_Engine(
                        index, nostem, score::bm25, taat_traversal, trec_id, run_id, out);
                } else if (score_function == "ql") {
                    return Query_Engine(index,
                                        nostem,
                                        score::query_likelihood,
                                        taat_traversal,
                                        trec_id,
                                        run_id,
                                        out);
                } else {
                    throw std::runtime_error("unknown score function type");
                }
            case Traversal_Type::DAAT:
                if (score_function == "bm25") {
                    return Query_Engine(
                        index, nostem, score::bm25, taat_traversal, trec_id, run_id, out);
                } else if (score_function == "ql") {
                    return Query_Engine(index,
                                        nostem,
                                        score::query_likelihood,
                                        taat_traversal,
                                        trec_id,
                                        run_id,
                                        out);
                } else {
                    throw std::runtime_error("unknown score function type");
                }
            }
            throw std::runtime_error("unknown traversal type");
        }
    }

private:
    struct Engine
    {
        Engine() = default;
        Engine(Engine const&) = default;
        Engine(Engine&&) noexcept = default;
        Engine& operator=(Engine const&) = default;
        Engine& operator=(Engine&&) noexcept = default;
        virtual ~Engine() = default;
        [[nodiscard]] virtual Query_Result_List
        run_query(gsl::span<std::string const> query_terms, int k) = 0;
    };

    template<class Index, class Score_Tag, class Traversal_Tag>
    class Impl : public Engine {
    public:
        explicit Impl(Index const& index,
                      bool nostem,
                      Score_Tag score_tag,
                      Traversal_Tag traversal_tag,
                      std::optional<int> trec_id,
                      std::string run_id,
                      std::ostream& out)
            : index_(index),
              nostem_(nostem),
              scorer_(score_tag),
              traversal_tag_(traversal_tag),
              trec_id_(trec_id),
              run_id_(std::move(run_id)),
              out_(out)
        {}

        [[nodiscard]] auto run_query_with_precomputed(gsl::span<std::string const> query_terms,
                                                      int const k,
                                                      Index const& index,
                                                      Traversal_Tag traversal_tag)
        {
            if constexpr (std::is_same_v<Traversal_Tag, irk::Taat_Traveral_Tag>) {
                return irk::taat(gsl::make_span(fetched_query_scored_postings(index, query_terms)),
                                 index.collection_size(),
                                 k);
            } else if constexpr (std::is_same_v<Traversal_Tag, irk::Daat_Traveral_Tag>) {
                return irk::daat(gsl::make_span(fetched_query_scored_postings(index, query_terms)),
                                 k);
            }
            std::clog << "unimplemented traversal tag: " << traversal_tag << '\n';
            std::abort();
        }

        [[nodiscard]] auto run_query_with_scoring(gsl::span<std::string const> query_terms,
                                                  int const k,
                                                  const Index& index,
                                                  Score_Tag score_tag,
                                                  Traversal_Tag traversal_tag)
        {
            const auto scorers = fetch_scorers(index, query_terms, score_tag);
            const auto postings = fetched_query_postings(index, query_terms);
            assert(scorers.size() == postings.size());
            if constexpr (std::is_same_v<Traversal_Tag, irk::Taat_Traveral_Tag>) {
                return irk::taat(
                    gsl::make_span(postings), gsl::make_span(scorers), index.collection_size(), k);
            } else if constexpr (std::is_same_v<Traversal_Tag, irk::Daat_Traveral_Tag>) {
                return irk::daat(gsl::make_span(postings), gsl::make_span(scorers), k);
            }
            std::clog << "unimplemented traversal tag: " << traversal_tag << '\n';
            std::abort();
        }

        template<class Document, class Score>
        void print_results(gsl::span<std::pair<Document, Score>> const& results)
        {
            auto const& titles = index_.titles();
            int rank = 0;
            for (auto& result : results) {
                print_(rank++, result.first, result.second);
            }
        }

        [[nodiscard]] Query_Result_List
        run_query(gsl::span<std::string const> query_terms, int k) override
        {
            std::vector<std::string> stemmed_terms;
            if (not nostem_) {
                irk::porter2_stemmer stem{};
                irk::transform_range(query_terms,
                                     std::back_inserter(stemmed_terms),
                                     [&](auto const& term) { return stem(term); });
            }
            if constexpr (std::is_base_of_v<score::scoring_function_tag, Score_Tag>) {
                auto results = run_query_with_scoring(
                    query_terms, k, index_, scorer_, traversal_tag_);
                return Query_Result_List(std::move(results));
            } else {
                auto results = run_query_with_precomputed(query_terms, k, index_, traversal_tag_);
                return Query_Result_List(std::move(results));
            }
        }

    private:
        Index const& index_;
        bool nostem_;
        Score_Tag scorer_;
        Traversal_Tag traversal_tag_;
        std::optional<int> trec_id_;
        std::string run_id_;
        std::ostream& out_;
    };

private:
    std::shared_ptr<Engine> self_;
};

}  // namespace irk

template<>
struct fmt::formatter<irk::Printable> : formatter<string_view> {
    template<typename FormatContext>
    auto format(irk::Printable p, FormatContext& ctx)
    {
        std::ostringstream os;
        os << p;
        return formatter<string_view>::format(os.str(), ctx);
    }
};
