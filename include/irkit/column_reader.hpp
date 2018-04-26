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
