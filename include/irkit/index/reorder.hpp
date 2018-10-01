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

#include <boost/iterator/permutation_iterator.hpp>
#include <boost/range/iterator_range.hpp>

#include <irkit/assert.hpp>
#include <irkit/index.hpp>
#include <irkit/index/source.hpp>
#include <irkit/index/types.hpp>

namespace irk::reorder {

using index::document_t;
using index::frequency_t;
using index::term_id_t;

template<typename Table>
auto sizes(const Table& size_table, const std::vector<document_t>& permutation)
{
    using size_type = typename Table::value_type;
    std::vector<size_type> sizes(size_table.begin(), size_table.end());
    std::vector<size_type> new_sizes(
        boost::make_permutation_iterator(sizes.begin(), permutation.begin()),
        boost::make_permutation_iterator(sizes.end(), permutation.end()));
    return irk::build_compact_table<size_type>(new_sizes);
}

template<typename Lexicon>
auto titles(
    const Lexicon& titles,
    const std::vector<document_t>& permutation,
    std::optional<int> keys_per_block = std::nullopt)
{
    std::vector<std::string> title_vec(titles.begin(), titles.end());
    return irk::build_lexicon(
        boost::make_permutation_iterator(
            title_vec.begin(), permutation.begin()),
        boost::make_permutation_iterator(title_vec.end(), permutation.end()),
        title_vec.begin(),
        title_vec.end(),
        keys_per_block.value_or(titles.keys_per_block()));
}

template<typename Permutation>
std::vector<document_t> docmap(const Permutation& permutation, size_t count)
{
    std::vector<document_t> map(count, std::numeric_limits<document_t>::max());
    document_t id(0);
    for (const auto& doc : permutation) {
        map[doc] = id++;
    }
    return map;
}

template<typename List>
std::vector<int>
compute_mask(const List& documents, const std::vector<document_t>& docmap)
{
    std::vector<std::pair<document_t, int>> remapped(documents.size());
    int n = 0;
    std::transform(
        std::begin(documents),
        std::end(documents),
        std::begin(remapped),
        [&docmap, &n](const auto& doc) {
            return std::make_pair(docmap[doc], n++);
        });
    std::sort(
        std::begin(remapped),
        std::end(remapped),
        [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
    auto last = std::find_if(
        std::begin(remapped), std::end(remapped), [](const auto& p) {
            return p.first == std::numeric_limits<document_t>::max();
        });
    std::vector<int> mask(std::distance(remapped.begin(), last));
    std::transform(
        std::begin(remapped), last, std::begin(mask), [](const auto& p) {
            return p.second;
        });
    return mask;
}

template<typename Range, typename Builder>
std::streamsize write_score_list(
    Builder& builder,
    const Range& values,
    const std::vector<int>& mask,
    std::ostream& os)
{
    auto it =
        boost::make_permutation_iterator(std::begin(values), std::begin(mask));
    auto end =
        boost::make_permutation_iterator(std::end(values), std::end(mask));
    for (; it != end; ++it) {
        builder.add(*it);
    }
    return builder.write(os);
}

template<typename T, bool delta, typename Range>
std::streamsize write_document_list(
    const Range& values,
    const std::vector<int>& mask,
    std::ostream& os,
    int block_size,
    const std::vector<document_t>& map)
{
    irk::index::block_list_builder<T, irk::stream_vbyte_codec<T>, delta>
        builder(block_size);
    std::vector<T> v(values.begin(), values.end());
    auto it = boost::make_permutation_iterator(std::begin(v), std::begin(mask));
    auto end = boost::make_permutation_iterator(std::end(v), std::end(mask));
    for (; it != end; ++it) {
        builder.add(map[*it]);
    }
    return builder.write(os);
}

template<typename T, bool delta, typename Range>
std::pair<std::streamsize, frequency_t> write_freq_list(
    const Range& values,
    const std::vector<int>& mask,
    std::ostream& os,
    int block_size)
{
    irk::index::block_list_builder<T, irk::stream_vbyte_codec<T>, delta>
        builder(block_size);
    std::vector<T> v(values.begin(), values.end());
    auto it = boost::make_permutation_iterator(std::begin(v), std::begin(mask));
    auto end = boost::make_permutation_iterator(std::end(v), std::end(mask));
    frequency_t occurrences(0);
    for (; it != end; ++it) {
        builder.add(*it);
        occurrences += *it;
    }
    return {builder.write(os), occurrences};
}

template<typename T, bool delta, typename Range>
std::streamsize write_score_list(
    const Range& values,
    const std::vector<int>& mask,
    std::ostream& os,
    int block_size)
{
    irk::index::block_list_builder<T, irk::stream_vbyte_codec<T>, delta>
        builder(block_size);
    std::vector<T> v(values.begin(), values.end());
    return write_score_list(builder, v, mask, os);
}

template<typename Index>
void postings(
    const Index& index,
    const std::vector<document_t>& map,
    std::ostream& term_freq_os,
    std::ostream& term_occ_os,
    std::ostream& document_os,
    std::ostream& document_offsets_os,
    std::ostream& frequency_os,
    std::ostream& frequency_offsets_os,
    const std::vector<std::string>& score_names,
    const std::vector<std::reference_wrapper<std::ostream>>& scores_os,
    const std::vector<std::reference_wrapper<std::ostream>>& scores_offset_os)
{
    int score_functions = scores_os.size();
    std::vector<frequency_t> frequencies(index.term_count());
    std::vector<frequency_t> occurrences(index.term_count());

    size_t doff(0);
    std::vector<size_t> document_offsets;

    size_t foff(0);
    std::vector<size_t> frequency_offsets;

    std::vector<size_t> soff(score_functions, 0u);
    std::vector<std::vector<size_t>> scores_offsets(score_functions);

    for (term_id_t term(0); term < index.term_count(); ++term) {
        auto documents = index.documents(term);
        auto mask = compute_mask(documents, map);
        auto block_size = documents.block_size();
        occurrences[term] = 0;
        document_offsets.push_back(doff);
        frequency_offsets.push_back(foff);
        doff += write_document_list<document_t, true>(
            documents, mask, document_os, block_size, map);
        auto [fsize, occ] = write_freq_list<frequency_t, false>(
            index.frequencies(term), mask, frequency_os, block_size);
        foff += fsize;
        occurrences[term] = occ;
        for (size_t idx = 0; idx < score_functions; ++idx) {
            scores_offsets[idx].push_back(soff[idx]);
            soff[idx] += write_score_list<typename Index::score_type, false>(
                index.scores(term, score_names[idx]),
                mask,
                scores_os[idx],
                block_size);
        }
        frequencies[term] = mask.size();
    }
    document_offsets_os << irk::build_offset_table(document_offsets);
    frequency_offsets_os << irk::build_offset_table(frequency_offsets);
    term_freq_os << irk::build_compact_table(frequencies);
    term_occ_os << irk::build_compact_table(occurrences);
    for (size_t idx = 0; idx < score_functions; ++idx) {
        scores_offset_os[idx] << irk::build_offset_table(scores_offsets[idx]);
    }
}

void index(
    const path& input_dir,
    const path& output_dir,
    const std::vector<int>& permutation)
{
    std::ofstream term_freq_os(irk::index::term_doc_freq_path(output_dir));
    std::ofstream term_occ_os(irk::index::term_occurrences_path(output_dir));
    std::ofstream document_os(irk::index::doc_ids_path(output_dir));
    std::ofstream document_offsets_os(irk::index::doc_ids_off_path(output_dir));
    std::ofstream frequency_os(irk::index::doc_counts_path(output_dir));
    std::ofstream frequency_offsets_os(
        irk::index::doc_counts_off_path(output_dir));
    std::ofstream titles_os(irk::index::titles_path(output_dir));
    std::ofstream title_map_os(irk::index::title_map_path(output_dir));
    std::ofstream sizes_os(irk::index::doc_sizes_path(output_dir));

    std::vector<std::ofstream> scp;
    std::vector<std::ofstream> sco;
    std::vector<std::reference_wrapper<std::ostream>> ref_scp;
    std::vector<std::reference_wrapper<std::ostream>> ref_sco;
    auto score_functions = index::all_score_names(input_dir);
    for (const auto& score : score_functions) {
        scp.emplace_back(
            (output_dir / fmt::format("{}.scores", score)).c_str());
        sco.emplace_back(
            (output_dir / fmt::format("{}.offsets", score)).c_str());
        ref_scp.emplace_back(scp.back());
        ref_sco.emplace_back(sco.back());
        boost::filesystem::copy(
            input_dir / fmt::format("{}.maxscore", score),
            output_dir / fmt::format("{}.maxscore", score));
    }

    auto source =
        inverted_index_disk_data_source::from(input_dir, score_functions)
            .value();
    inverted_index_view index(&source);
    auto rtitles = irk::reorder::titles(index.titles(), permutation);
    rtitles.serialize(title_map_os);
    irk::io::write_lines(rtitles, titles_os);
    boost::filesystem::copy(
        irk::index::terms_path(input_dir), irk::index::terms_path(output_dir));
    boost::filesystem::copy(
        irk::index::term_map_path(input_dir),
        irk::index::term_map_path(output_dir));
    boost::filesystem::copy(
        irk::index::properties_path(input_dir),
        irk::index::properties_path(output_dir));
    irk::reorder::sizes(index.document_sizes(), permutation).serialize(sizes_os);
    irk::reorder::postings(
        index,
        irk::reorder::docmap(permutation, index.collection_size()),
        term_freq_os,
        term_occ_os,
        document_os,
        document_offsets_os,
        frequency_os,
        frequency_offsets_os,
        score_functions,
        ref_scp,
        ref_sco);
}

};
