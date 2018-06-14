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

//! \file types.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <ostream>
#include <utility>
#include <boost/dynamic_bitset.hpp>

namespace irk {

using byte = unsigned char;
using bitword = boost::dynamic_bitset<byte>;

template<class T, class U>
using enable_if_equal = typename std::enable_if<std::is_same<T, U>::value>;

template<class T, class U>
using enable_if_not_equal = typename std::enable_if<!std::is_same<T, U>::value>;

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
struct _posting {
    Doc doc;
    Score score;
    bool operator==(const _posting<Doc, Score>& rhs) const
    {
        return doc == rhs.doc && score == rhs.score;
    }
};

template<class Doc, class Score>
std::ostream& operator<<(std::ostream& o, _posting<Doc, Score> posting)
{
    return o << posting.doc << ":" << posting.score;
}

};  // namespace irk
