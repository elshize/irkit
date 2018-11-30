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

#include <string>

#include <boost/filesystem.hpp>

#include <irkit/index.hpp>
#include <irkit/index/cluster.hpp>
#include <irkit/index/score.hpp>
#include <irkit/index/source.hpp>
#include <irkit/value.hpp>

namespace irk {

class Scoreable_Index {
public:
    template<class Source, class Index, class ScoreTag>
    explicit Scoreable_Index(std::shared_ptr<Source> source, Index index, ScoreTag tag)
        : self_(std::make_shared<Impl<Source, Index, ScoreTag>>(
              std::move(source), std::move(index), tag))
    {}

    [[nodiscard]] static Scoreable_Index
    from(boost::filesystem::path const& dir, std::string const& score_name)
    {
        auto props = index::Properties::read(dir);
        if (props.shard_count) {
            auto source = irk::Index_Cluster_Data_Source<irk::Inverted_Index_Mapped_Source>::from(
                dir);
            if (score_name == "bm25") {
                return Scoreable_Index(source, irk::Index_Cluster(source), irk::score::bm25);
            } else if (score_name == "ql") {
                return Scoreable_Index(
                    source, irk::Index_Cluster(source), irk::score::query_likelihood);
            } else {
                throw "unknown scorer";
            }
        } else {
            auto source = irtl::value(irk::Inverted_Index_Mapped_Source::from(dir));
            if (score_name == "bm25") {
                return Scoreable_Index(source, irk::inverted_index_view(source), irk::score::bm25);
            } else if (score_name == "ql") {
                return Scoreable_Index(
                    source, irk::inverted_index_view(source), irk::score::query_likelihood);
            } else {
                throw "unknown scorer";
            }
        }
    }

    nonstd::expected<void, std::string> calc_score_stats() { return self_->calc_score_stats(); }

private:
    struct Scoreable
    {
        Scoreable() = default;
        Scoreable(Scoreable const&) = default;
        Scoreable(Scoreable&&) noexcept = default;
        Scoreable& operator=(Scoreable const&) = default;
        Scoreable& operator=(Scoreable&&) noexcept = default;
        virtual ~Scoreable() = default;
        virtual nonstd::expected<void, std::string> calc_score_stats() = 0;
    };

    template<class Source, class Index, class ScoreTag>
    class Impl : public Scoreable {
    public:
        explicit Impl(std::shared_ptr<Source> source, Index index, ScoreTag)
            : source_(std::move(source)), index_(std::move(index))
        {}
        nonstd::expected<void, std::string> calc_score_stats() override
        {
            for (auto const& shard : index_.shards()) {
                using term_id_type = typename Index::term_id_type;
                irk::index::detail::ScoreStatsFn fn{shard.dir(), std::string{ScoreTag{}}};
                std::vector<term_id_type> term_ids(shard.term_count());
                std::iota(term_ids.begin(), term_ids.end(), term_id_type{0});
                fn(term_ids, [&](term_id_type id) {
                    return shard.postings(id).scored(shard.term_scorer(id, ScoreTag{}));
                });
            }
            return nonstd::expected<void, std::string>();
        }

    private:
        std::shared_ptr<Source> source_;
        Index index_;
    };

private:
    std::shared_ptr<Scoreable> self_;
};

}  // namespace irk
