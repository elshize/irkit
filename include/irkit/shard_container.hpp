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

#include <boost/te.hpp>
#include <irkit/index.hpp>
#include <irkit/index/cluster.hpp>

namespace irk {

struct Shard_Container : boost::te::poly<Shard_Container> {
    using boost::te::poly<Shard_Container>::poly;

    [[nodiscard]] auto shards() const -> gsl::span<irk::inverted_index_view const>
    {
        return boost::te::call<gsl::span<irk::inverted_index_view const>>(
            [](auto const& self) { return self.shards(); }, *this);
    }

    [[nodiscard]] static Shard_Container
    from(boost::filesystem::path const& dir, gsl::span<std::string const> scores = {})
    {
        auto props = irk::index::Properties::read(dir);
        if (props.shard_count) {
            auto source = irk::Index_Cluster_Data_Source<irk::Inverted_Index_Mapped_Source>::from(
                dir, filter_quantized(scores));
            return irk::Index_Cluster(source);
        } else {
            auto source = irtl::value(
                irk::Inverted_Index_Mapped_Source::from(dir, filter_quantized(scores)));
            return irk::inverted_index_view(source);
        }
    }

    [[nodiscard]] static std::vector<std::string>
    filter_quantized(gsl::span<std::string const> names)
    {
        std::vector<std::string> filtered;
        std::copy_if(
            names.begin(), names.end(), std::back_inserter(filtered), [](auto const& name) {
                return std::find(name.begin(), name.end(), '-') != name.end();
            });
        return filtered;
    }
};

}  // namespace irk
