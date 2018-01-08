#pragma once

#include <utility>

namespace irkit {

//! The type of the element of Range.
template<class Range>
using element_t = decltype(*std::declval<Range>().begin());

//! The type of the element of Range stripped of `const` and `&`.
template<class Range>
using pure_element_t = std::remove_const_t<
    std::remove_reference_t<decltype(*std::declval<Range>().begin())>>;

template<class Range>
using iterator_t = decltype(std::declval<Range>().begin());

template<class Range>
using const_iterator_t = decltype(std::declval<Range>().cbegin());

template<class Posting>
using doc_t = decltype(std::declval<Posting>().doc);

template<class Posting>
using score_t = decltype(std::declval<Posting>().score);

template<class Doc, class Score>
struct _Posting {
    Doc doc;
    Score score;
};

};  // namespace irkit
