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
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <memory>
#include <string>

#include <irkit/parsing/snowball/porter2.hpp>

namespace irk {

class porter2_stemmer {
public:
    porter2_stemmer() = default;
    porter2_stemmer(const porter2_stemmer& /* other */)
        : env_(porter2::create_env())
    {}
    porter2_stemmer& operator=(const porter2_stemmer& /* other */)
    {
        env_ = porter2::create_env();
        return *this;
    }
    porter2_stemmer(porter2_stemmer&&) noexcept = default;
    porter2_stemmer& operator=(porter2_stemmer&&) noexcept = default;
    ~porter2_stemmer() { porter2::close_env(env_); }

    std::string stem(const std::string& word) const
    {
        porter2::SN_set_current(env_,
            word.size(),
            reinterpret_cast<const unsigned char*>(word.c_str()));
        porter2::stem(env_);
        auto length = env_->l;  // NOLINT
        return std::string(env_->p, std::next(env_->p, length));
    }

private:
    porter2::SN_env* env_ = porter2::create_env();
};

};  // namespace irk
