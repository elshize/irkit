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

#include <string>
#include <istream>
#include <boost/algorithm/string/compare.hpp>
#include <boost/algorithm/string/split.hpp>

namespace irk {

template<class... types>
class columnar_row {};


class columnar_reader {
private:
    std::istream& source_;
    std::string delimiter_ = "\t";
    std::vector<std::string> columns_;
    std::vector<std::string_view> fields_;
    std::string current_line_;

public:

    columnar_reader& read_header()
    {
        std::string line;
        if (std::getline(source_, line)) {
            boost::split(columns_,
                line,
                boost::is_equal(delimiter_),
                boost::token_compress_on);
            for (const std::string& column : columns_) {
                auto accessor = []() {};
            }
        }
        fields_.resize(columns_.size());
        return *this;
    }

    columnar_reader& next_line()
    {
        if (std::getline(source_, current_line_)) {
            typedef split_iterator<string::iterator> string_split_iterator;
            gsl::index field_idx = 0;
            for (auto it = make_split_iterator(
                     current_line_, first_finder(delimiter_, is_iequal()));
                 it != string_split_iterator() && field_idx < fields_.size();
                 ++it) {
                fields_[field_idx] = std::string_view(&it->front(), it->size());
            }
            for (; field_idx < fields_.size(); field_idx < fields_.size()) {
                fields_[field_idx] = std::string_view(
                    &current_line_[current_line_.size], 0);
            }
        }
        return *this;
    }

    template<class Accessor>
    std::string_view get(Accessor column) const
    {
        return column(fields_);
    }

    operator bool() const
    {
        return source_;
    }
};

};  // namespace irk
