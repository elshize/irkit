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

#include <algorithm>
#include <bitset>
#include <chrono>
#include <string>
#include <vector>
#include <random>

#include <CLI/CLI.hpp>
#include <boost/range/algorithm.hpp>

#include <irkit/coding/stream_vbyte.hpp>
#include <irkit/coding/vbyte.hpp>

using std::chrono::seconds;
using std::chrono::nanoseconds;
using std::chrono::steady_clock;
using std::chrono::duration_cast;

int main(int argc, char** argv)
{
    int count = 100'000'000;
    int max_val = 10'000;
    long seed = 987654321;

    CLI::App app{"Varbyte coding benchmark"};
    app.add_option("--count", count, "Number of integers to process", true);
    app.add_option("--max-val", max_val, "Time limit per posting in ns", true);
    app.add_option("--seed", seed, "Time limit per posting in ns", true);

    CLI11_PARSE(app, argc, argv);
    std::default_random_engine generator(seed);
    std::uniform_int_distribution<int> distribution(1, max_val);
    irk::stream_vbyte_codec<std::uint32_t> codec;

    std::vector<std::uint32_t> random_numbers;
    random_numbers.reserve(count);
    std::generate_n(std::back_inserter(random_numbers), count, [&]() {
        return distribution(generator);
    });
    std::sort(std::begin(random_numbers), std::end(random_numbers));

    std::vector<std::uint8_t> sink(count * 4);
    nanoseconds encode_elapsed(0);
    auto start = steady_clock::now();
    codec.encode(
        std::begin(random_numbers), std::end(random_numbers), sink.begin());
    encode_elapsed += duration_cast<nanoseconds>(steady_clock::now() - start);

    std::vector<std::uint32_t> decoded(count);
    nanoseconds decode_elapsed(0);
    start = steady_clock::now();
    codec.decode(std::begin(sink), std::begin(decoded), count);
    decode_elapsed += duration_cast<nanoseconds>(steady_clock::now() - start);

    for (int i = 0; i < count; ++i)
    {
        if (random_numbers[i] != decoded[i])
        {
            std::cout << "[" << i << "] " << random_numbers[i]
                      << " != " << decoded[i] << std::endl;
            return 1;
        }
    }
    auto avg = std::accumulate(random_numbers.begin(), random_numbers.end(), 0L)
        / count;
    auto encode_ns_per_int = (double)encode_elapsed.count() / count;
    auto decode_ns_per_int = (double)decode_elapsed.count() / count;
    std::cout << "Average number encoded: " << avg << std::endl;
    std::cout << "Encoding: " << encode_ns_per_int << " ns/int" << std::endl;
    std::cout << "Encoding: " << 1'000 / encode_ns_per_int << " mln int/s"
              << std::endl;
    std::cout << "Decoding: " << decode_ns_per_int << " ns/int" << std::endl;
    std::cout << "Decoding: " << 1'000 / decode_ns_per_int << " mln int/s"
              << std::endl;

    return 0;
}
