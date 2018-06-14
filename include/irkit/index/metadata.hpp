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

//! \file metadata.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <boost/filesystem.hpp>
#include <irkit/index.hpp>

namespace irk::index {

namespace fs = boost::filesystem;

//! Container for index-related metadata.
struct metadata {
    fs::path dir;
    fs::path doc_ids = irk::index::doc_ids_path(dir);
    fs::path doc_ids_off = irk::index::doc_ids_off_path(dir);
    fs::path doc_counts = irk::index::doc_counts_path(dir);
    fs::path doc_counts_off = irk::index::doc_counts_off_path(dir);
    fs::path terms = irk::index::terms_path(dir);
    fs::path term_doc_freq = irk::index::term_doc_freq_path(dir);
    fs::path doc_titles = irk::index::titles_path(dir);

    metadata(fs::path dir) : dir(dir) {}
};

};  // namespace irk::index
