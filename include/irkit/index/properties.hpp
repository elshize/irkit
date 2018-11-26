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

#include <algorithm>
#include <bitset>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/range/adaptors.hpp>
#include <cppitertools/itertools.hpp>
#include <fmt/format.h>
#include <gsl/span>
#include <nlohmann/json.hpp>
#include <range/v3/utility/concepts.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <taily.hpp>
#include <type_safe/config.hpp>
#include <type_safe/index.hpp>
#include <type_safe/strong_typedef.hpp>
#include <type_safe/types.hpp>

#include <irkit/algorithm.hpp>
#include <irkit/assert.hpp>
#include <irkit/coding.hpp>
#include <irkit/coding/stream_vbyte.hpp>
#include <irkit/compacttable.hpp>
#include <irkit/daat.hpp>
#include <irkit/index/posting_list.hpp>
#include <irkit/index/types.hpp>
#include <irkit/io.hpp>
#include <irkit/lexicon.hpp>
#include <irkit/memoryview.hpp>
#include <irkit/quantize.hpp>
#include <irkit/score.hpp>
#include <irkit/types.hpp>

namespace ts = type_safe;

namespace irk {

using index::frequency_t;
using index::offset_t;
using index::term_id_t;

template<
    typename P,
    typename O = P,
    typename M = O,
    typename E = M,
    typename V = E>
struct score_tuple {
    P postings;
    O offsets;
    M max_scores;
    E exp_values;
    V variances;
};

namespace index {

    template<class T>
    T read_property(nlohmann::json& properties, std::string_view name)
    {
        if (auto pos = properties.find(name); pos != properties.end()) {
            return *pos;
        }
        throw std::runtime_error(fmt::format("property {} not found", name));
    }

    using boost::adaptors::filtered;
    using boost::filesystem::directory_iterator;
    using boost::filesystem::is_regular_file;
    using boost::filesystem::path;

    struct posting_paths {
        path postings;
        path offsets;
        path max_scores;
    };

    inline path properties_path(const path& dir)
    { return dir / "properties.json"; }
    inline path doc_ids_path(const path& dir)
    { return dir / "doc.id"; }
    inline path doc_ids_off_path(const path& dir)
    { return dir / "doc.idoff"; }
    inline path doc_counts_path(const path& dir)
    { return dir / "doc.count"; }
    inline path doc_counts_off_path(const path& dir)
    { return dir / "doc.countoff"; }
    inline path terms_path(const path& dir)
    { return dir / "terms.txt"; }
    inline path term_map_path(const path& dir)
    { return dir / "terms.map"; }
    inline path term_doc_freq_path(const path& dir)
    { return dir / "terms.docfreq"; }
    inline path titles_path(const path& dir)
    { return dir / "titles.txt"; }
    inline path title_map_path(const path& dir)
    { return dir / "titles.map"; }
    inline path doc_sizes_path(const path& dir)
    { return dir / "doc.sizes"; }
    inline path term_occurrences_path(const path& dir)
    { return dir / "term.occurrences"; }
    inline path score_offset_path(const path& dir, const std::string& name)
    { return dir / fmt::format("{}.offsets", name); }
    inline path max_scores_path(const path& dir, const std::string& name)
    { return dir / fmt::format("{}.maxscore", name); }

    inline score_tuple<path>
    score_paths(const path& dir, const std::string& name)
    {
        return {dir / fmt::format("{}.scores", name),
                dir / fmt::format("{}.offsets", name),
                dir / fmt::format("{}.maxscore", name),
                dir / fmt::format("{}.expscore", name),
                dir / fmt::format("{}.varscore", name)};
    }

    inline std::vector<std::string> all_score_names(const path& dir)
    {
        std::vector<std::string> names;
        auto matches = [&](const path& p) {
            return boost::algorithm::ends_with(
                p.filename().string(), ".scores");
        };
        for (auto& file :
             boost::make_iterator_range(directory_iterator(dir), {}))
        {
            if (is_regular_file(file.path()) && matches(file.path())) {
                std::string filename = file.path().filename().string();
                std::string name(
                    filename.begin(),
                    std::find(filename.begin(), filename.end(), '.'));
                names.push_back(name);
            }
        }
        return names;
    }

    struct QuantizationProperties {
        double min{};
        double max{};
        int32_t nbits{};
    };

    struct Properties {
        int32_t skip_block_size{};
        int64_t occurrences_count{};
        int32_t document_count{};
        double avg_document_size{};
        int32_t max_document_size{};
        std::unordered_map<std::string, QuantizationProperties>
            quantized_scores{};
    };

    Properties read_properties(const path& index_dir)
    {
        Properties properties;
        std::ifstream ifs(properties_path(index_dir).c_str());
        nlohmann::json jprop;
        ifs >> jprop;
        properties.document_count = read_property<int32_t>(jprop, "documents");
        properties.occurrences_count =
            index::read_property<int64_t>(jprop, "occurrences");
        properties.skip_block_size =
            index::read_property<int32_t>(jprop, "skip_block_size");
        properties.avg_document_size =
            index::read_property<double>(jprop, "avg_document_size");
        properties.max_document_size =
            index::read_property<int32_t>(jprop, "max_document_size");
        return properties;
    }

    void save_properties(const Properties& properties, const path& index_dir)
    {
        std::ofstream ofs(properties_path(index_dir).c_str());
        nlohmann::json jprop;
        jprop["documents"] = properties.document_count;
        jprop["occurrences"] = properties.occurrences_count;
        jprop["skip_block_size"] = properties.skip_block_size;
        jprop["avg_document_size"] = properties.avg_document_size;
        jprop["max_document_size"] = properties.max_document_size;
        ofs << jprop;
    }

}  // namespace index
};
