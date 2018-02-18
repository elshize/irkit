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

//! \file concpets.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <boost/concept/assert.hpp>
#include <boost/concept_check.hpp>
#include "irkit/types.hpp"

namespace irk::concept {

template<class R>
struct InputRange {
public:
    typedef iterator_t<R> iterator_type;
    BOOST_CONCEPT_USAGE(InputRange)
    {
        R p(r);
        same_type(r.begin(), v);
        same_type(r.end(), v);
    }
 private:
    R r;
    iterator_type v;

    template <typename T>
    void same_type(T const&, T const&);
};

};  // namespace irk::concept
