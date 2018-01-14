#pragma once

#include <ostream>
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
    bool operator==(const _Posting<Doc, Score>& rhs) const
    {
        return doc == rhs.doc && score == rhs.score;
    }
};

template<class Doc, class Score>
std::ostream& operator<<(std::ostream& o, _Posting<Doc, Score> posting)
{
    return o << posting.doc << ":" << posting.score;
}


};  // namespace irkit
