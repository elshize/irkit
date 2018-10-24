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

#include <functional>
#include <numeric>
#include <vector>

#include <boost/iterator/iterator_facade.hpp>

#include <irkit/assert.hpp>
#include <irkit/movingrange.hpp>

namespace irk {

template<class DocumentListT, class PayloadListT, class ScoreFn>
class scored_posting_list_view;

// TODO(michal): Make variable numbers of payloads
template<class DocumentListT, class PayloadListT>
class posting_list_view {
public:
    using document_list_type = DocumentListT;
    using payload_list_type = PayloadListT;
    using document_type = typename document_list_type::value_type;
    using payload_type = typename payload_list_type::value_type;
    using difference_type = std::int32_t;
    using document_iterator_t =
        decltype(std::declval<const document_list_type>().begin());
    using payload_iterator_t =
        decltype(std::declval<const payload_list_type>().begin());

    class posting_view {
    public:
        posting_view(const document_iterator_t& document_iterator,
            const payload_iterator_t& payload_iterator)
            : document_iterator_(std::cref(document_iterator)),
              payload_iterator_(std::cref(payload_iterator))
        {}
        posting_view(const posting_view&) = default;
        posting_view(posting_view&&) noexcept = default;
        posting_view& operator=(const posting_view&) = default;
        posting_view& operator=(posting_view&&) noexcept = default;
        const document_type& document() const
        {
            return *(document_iterator_.get());
        }
        const payload_type& payload() const
        {
            return *(payload_iterator_.get());
        }
        explicit operator std::pair<document_type, payload_type>() const
        {
            return std::pair(document(), payload());
        }
        explicit operator std::tuple<document_type, payload_type>() const
        {
            return std::pair(document(), payload());
        }

    private:
        std::reference_wrapper<const document_iterator_t> document_iterator_;
        std::reference_wrapper<const payload_iterator_t> payload_iterator_;
    };

    class iterator : public boost::iterator_facade<
                         iterator,
                         const posting_view,
                         boost::forward_traversal_tag> {
    public:
        iterator(document_iterator_t document_iterator,
            payload_iterator_t payload_iterator)
            : document_iterator_(std::move(document_iterator)),
              payload_iterator_(std::move(payload_iterator)),
              current_posting_(document_iterator_, payload_iterator_)
        {}
        iterator(const iterator& other)
            : document_iterator_(other.document_iterator_),
              payload_iterator_(other.payload_iterator_),
              current_posting_(document_iterator_, payload_iterator_)
        {}
        iterator(iterator&& other) noexcept
            : document_iterator_(other.document_iterator_),
              payload_iterator_(other.payload_iterator_),
              current_posting_(document_iterator_, payload_iterator_)
        {}
        iterator& operator=(const iterator& other)
        {
            document_iterator_ = other.document_iterator_;
            payload_iterator_ = other.payload_iterator_;
            current_posting_ =
                posting_view(document_iterator_, payload_iterator_);
            return *this;
        }
        iterator& operator=(iterator&& other) noexcept
        {
            document_iterator_ = other.document_iterator_;
            payload_iterator_ = other.payload_iterator_;
            current_posting_ =
                posting_view(document_iterator_, payload_iterator_);
            return *this;
        }
        ~iterator() = default;

        iterator& moveto(document_type doc)
        {
            document_iterator_.moveto(doc);
            payload_iterator_.align(document_iterator_);
            return *this;
        }

        iterator nextgeq(document_type doc) const
        {
            iterator iter(*this);
            iter.moveto(doc);
            return iter;
        }

        document_type document() const { return *document_iterator_; }
        payload_type payload() const { return *payload_iterator_; }
        int idx() const { return document_iterator_.idx(); }
        const posting_view& current_posting() const { return current_posting_; }

    private:
        friend class boost::iterator_core_access;
        void increment()
        {
            ++document_iterator_;
            ++payload_iterator_;
        }
        void advance(difference_type n)
        {
            document_iterator_ += n;
            payload_iterator_ += n;
        }
        bool equal(const iterator& other) const
        {
            return document_iterator_ == other.document_iterator_;
        }

        const posting_view& dereference() const { return current_posting_; }

    public:
        document_iterator_t document_iterator_;
        payload_iterator_t payload_iterator_;
        posting_view current_posting_;
    };
    using const_iterator = iterator;

    posting_list_view(document_list_type documents, payload_list_type payloads)
        : documents_(std::move(documents)), payloads_(std::move(payloads))
    {
        EXPECTS(documents_.size() == payloads_.size());
    }
    posting_list_view(const posting_list_view&) = default;
    posting_list_view(posting_list_view&&) noexcept = default;
    posting_list_view& operator=(const posting_list_view&) = default;
    posting_list_view& operator=(posting_list_view&&) noexcept = default;

    iterator begin() const
    {
        return iterator(documents_.begin(), payloads_.begin());
    };
    iterator end() const
    {
        return iterator(documents_.end(), payloads_.end());
    };
    iterator lookup(document_type id) const { return begin().nextgeq(id); };
    difference_type size() const { return documents_.size(); }

    const document_list_type& document_list() const { return documents_; }
    const payload_list_type& payload_list() const { return payloads_; }
    auto block_size() const { return documents_.block_size(); }

    auto
    scored(std::function<double(document_type, payload_type)> score_fn) const
    {
        return scored_posting_list_view(*this, std::move(score_fn));
    }

private:
    document_list_type documents_;
    payload_list_type payloads_;
};

template<class DocumentListT, class PayloadListT, class ScoreFn>
class scored_posting_list_view {
public:
    using document_list_type = DocumentListT;
    using payload_list_type = PayloadListT;
    using unscored_type =
        posting_list_view<document_list_type, payload_list_type>;
    using unscored_iterator_type =
        typename posting_list_view<document_list_type, payload_list_type>::
            iterator;
    using document_type = typename document_list_type::value_type;
    using payload_type = typename payload_list_type::value_type;
    using difference_type = std::int32_t;
    using document_iterator_t =
        decltype(std::declval<const document_list_type>().begin());
    using payload_iterator_t =
        decltype(std::declval<const payload_list_type>().begin());

    class posting_view {
    public:
        posting_view(
            const typename unscored_type::posting_view& posting,
            const ScoreFn& score_fn)
            : posting_(std::cref(posting)), score_fn_(std::cref(score_fn))
        {}
        posting_view(const posting_view&) = default;
        posting_view(posting_view&&) noexcept = default;
        posting_view& operator=(const posting_view&) = default;
        posting_view& operator=(posting_view&&) noexcept = default;
        const document_type& document() const
        {
            return posting_.get().document();
        }
        const payload_type& unscored_payload() const
        {
            return posting_.get().payload();
        }
        double score() const { return payload(); }
        double payload() const
        {
            return score_fn_(document(), unscored_payload());
        }
        explicit operator std::pair<document_type, payload_type>() const
        {
            return std::pair(document(), payload());
        }
        explicit operator std::tuple<document_type, payload_type>() const
        {
            return std::pair(document(), payload());
        }

    private:
        std::reference_wrapper<const typename unscored_type::posting_view>
            posting_;
        std::reference_wrapper<const ScoreFn> score_fn_;
    };

    class iterator : public boost::iterator_facade<
                         iterator,
                         const posting_view,
                         boost::forward_traversal_tag> {
    public:
        iterator(unscored_iterator_type unscored, ScoreFn score_fn)
            : score_fn_(score_fn),
              unscored_(std::move(unscored)),
              current_posting_(unscored_.current_posting(), score_fn_)
        {}
        iterator(const iterator& other)
            : score_fn_(other.score_fn_),
              unscored_(other.unscored_),
              current_posting_(unscored_.current_posting(), score_fn_)
        {}
        iterator(iterator&& other) noexcept
            : score_fn_(other.score_fn_),
              unscored_(other.unscored_),
              current_posting_(unscored_.current_posting(), score_fn_)
        {}
        iterator& operator=(const iterator& other)
        {
            unscored_ = other.unscored_;
            score_fn_ = other.score_fn_;
            current_posting_ =
                posting_view(unscored_.current_posting(), score_fn_);
            return *this;
        }
        iterator& operator=(iterator&& other) noexcept
        {
            unscored_ = other.unscored_;
            score_fn_ = other.score_fn_;
            current_posting_ =
                posting_view(unscored_.current_posting(), score_fn_);
            return *this;
        }
        ~iterator() = default;

        iterator& moveto(document_type doc)
        {
            unscored_.moveto(doc);
            return *this;
        }

        iterator nextgeq(document_type doc) const
        {
            iterator iter(*this);
            iter.moveto(doc);
            return iter;
        }

        document_type document() const { return *unscored_.document(); }
        payload_type payload() const { return *unscored_.payload(); }
        int idx() const { return unscored_.idx(); }

    private:
        friend class boost::iterator_core_access;
        void increment() { ++unscored_; }
        void advance(difference_type n) { unscored_ += n; }
        bool equal(const iterator& other) const
        {
            return unscored_ == other.unscored_;
        }
        const posting_view& dereference() const { return current_posting_; }

    public:
        ScoreFn score_fn_;
        unscored_iterator_type unscored_;
        posting_view current_posting_;
    };
    using const_iterator = iterator;

    scored_posting_list_view(unscored_type posting_list, ScoreFn score_fn)
        : unscored_list_(std::move(posting_list)),
          score_fn_(std::move(score_fn))
    {}
    scored_posting_list_view(const scored_posting_list_view&) = default;
    scored_posting_list_view(scored_posting_list_view&&) noexcept = default;
    scored_posting_list_view&
    operator=(const scored_posting_list_view&) = default;
    scored_posting_list_view&
    operator=(scored_posting_list_view&&) noexcept = default;

    iterator begin() const
    {
        return iterator(unscored_list_.begin(), score_fn_);
    };
    iterator end() const { return iterator(unscored_list_.end(), score_fn_); };
    iterator lookup(document_type id) const { return begin().nextgeq(id); };
    difference_type size() const { return unscored_list_.size(); }

    const document_list_type& document_list() const
    {
        return unscored_list_.document_list();
    }
    const payload_list_type& payload_list() const
    {
        return unscored_list_.payload_list();
    }
    auto block_size() const { return unscored_list_.block_size(); }

private:
    unscored_type unscored_list_;
    ScoreFn score_fn_;
};

template<typename T>
using OrderFn = std::function<bool(const T&, const T&)>;

template<typename T, typename Iterator>
class UnionIterator : public boost::iterator_facade<
                          UnionIterator<T, Iterator>,
                          const T,
                          boost::forward_traversal_tag> {
public:
    using OrderFn = irk::OrderFn<moving_range<Iterator>>;
    UnionIterator(
        std::vector<moving_range<Iterator>> ranges,
        OrderFn order,
        size_t pos,
        size_t length)
        : ranges_(std::move(ranges)),
          order_(std::move(order)),
          pos_(pos),
          length_(length)
    {
        std::sort(ranges_.begin(), ranges_.end(), order_);
    }

private:
    friend class boost::iterator_core_access;
    void increment()
    {
        if (pos_ == length_) { return; }
        ranges_.front().advance();
        for (auto it = std::next(ranges_.begin()); it != ranges_.end(); ++it) {
            auto prev = std::prev(it);
            if (order_(*prev, *it)) { break; }
            std::iter_swap(prev, it);
        }
        pos_ += 1;
    }
    bool equal(const UnionIterator& other) const { return pos_ == other.pos_; }
    const T& dereference() const { return ranges_.front().front(); }

    std::vector<moving_range<Iterator>> ranges_;
    OrderFn order_;
    size_t pos_;
    size_t length_;
};

template<typename T, typename PostingList>
class Union {
    using list_iterator = typename PostingList::iterator;

    std::vector<moving_range<list_iterator>> retrieve_ranges(
        std::function<moving_range<list_iterator>(const PostingList&)> fun)
        const
    {
        std::vector<moving_range<list_iterator>> ranges;
        std::transform(
            lists_.begin(),
            lists_.end(),
            std::back_inserter(ranges),
            [&fun](const auto& list) { return fun(list); });
        return ranges;
    }

public:
    Union(
        std::vector<PostingList> lists,
        irk::OrderFn<moving_range<list_iterator>> order)
        : lists_(std::move(lists)), order_(std::move(order))
    {
        length_ = std::accumulate(
            lists_.begin(),
            lists_.end(),
            0,
            [](const auto acc, const auto& list) { return acc + list.size(); });
    }
    auto begin() const
    {
        return UnionIterator<T, list_iterator>(
            retrieve_ranges([](const auto& list) {
                return moving_range(list.begin(), list.end());
            }),
            order_,
            0,
            length_);
    }
    auto end() const
    {
        return UnionIterator<T, list_iterator>(
            retrieve_ranges([](const auto& list) {
                return moving_range(list.end(), list.end());
            }),
            order_,
            length_,
            length_);
    }
    auto size() const { return length_; }

private:
    std::vector<PostingList> lists_;
    irk::OrderFn<moving_range<list_iterator>> order_;
    size_t length_;
};

template<typename Iter>
auto merge(Iter first_posting_list, Iter last_posting_list)
{
    using list_type = std::remove_const_t<
        std::remove_reference_t<decltype(*first_posting_list)>>;
    using list_iterator = typename list_type::iterator;
    return Union<typename list_type::posting_view, list_type>(
        std::vector<list_type>(first_posting_list, last_posting_list),
        [](const moving_range<list_iterator>& lhs,
           const moving_range<list_iterator>& rhs) {
            if (rhs.empty()) {
                return true;
            }
            if (lhs.empty()) {
                return false;
            }
            return lhs.front().document() < rhs.front().document();
        });
}

template<typename Range>
auto merge(const Range& posting_lists)
{
    return merge(posting_lists.begin(), posting_lists.end());
}

}  // namespace irk
