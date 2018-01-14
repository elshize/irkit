#pragma once

#include <gsl/span>
#include <iostream>
#include <range/v3/range_concepts.hpp>
#include <range/v3/utility/concepts.hpp>
#include "irkit/types.hpp"
#include "irkit/utils.hpp"

namespace irkit {

template<class T, CONCEPT_REQUIRES_(ranges::Integral<T>())>
struct VarByte {
    void append_encoded(T n, std::vector<char>& bytes)
    {
        while (true) {
            bytes.push_back(n < 128 ? 128 + n : n % 128);
            if (n < 128) {
                break;
            }
            n /= 128;
        }
    }

    std::vector<char> encode(const std::vector<T>& ints)
    {
        std::vector<char> bytes;
        for (auto n : ints) {
            append_encoded(n, bytes);
        }
        return bytes;
    }

    std::vector<char> delta_encode(const std::vector<T>& ints)
    {
        T last(0);
        std::vector<char> bytes;
        for (auto n : ints) {
            append_encoded(n - last, bytes);
            last = n;
        }
        return bytes;
    }

    template<class Range,
        class TransformFn,
        CONCEPT_REQUIRES_(ranges::InputRange<Range>())>
    std::vector<char> encode(const Range& range, TransformFn fn)
    {
        std::vector<char> bytes;
        for (const auto& item : range) {
            append_encoded(fn(item), bytes);
        }
        return bytes;
    }

    template<class Iter, CONCEPT_REQUIRES_(ranges::ForwardIterator<Iter>())>
    Iter append_decoded(Iter it, Iter end, std::vector<T>& ints)
    {
        char b;
        int n = 0;
        unsigned short shift = 0;
        while (it != end) {
            b = *it++;
            int val = b & 0b01111111;
            n |= val << shift;
            shift += 7;
            if (val != b) {
                ints.push_back(n);
                return it;
            }
        }
        throw std::runtime_error(
            "reached end of byte range before end of value");
    }

    std::vector<T> decode(gsl::span<const char> bytes)
    {
        std::vector<T> ints;
        auto it = bytes.begin();
        auto end = bytes.end();
        while (it != end) {
            it = append_decoded(it, end, ints);
        }
        return ints;
    }

    std::vector<T> decode(const std::vector<char>& bytes)
    {
        return decode(gsl::span<const char>(bytes));
    }

    std::vector<T> delta_decode(gsl::span<const char> bytes)
    {
        std::vector<T> ints;
        auto it = bytes.begin();
        auto end = bytes.end();
        T last = T(0);
        while (it != end) {
            it = append_decoded(it, end, ints);
            ints.back() += last;
            last = ints.back();
        }
        return ints;
    }

    std::vector<T> delta_decode(const std::vector<char>& bytes)
    {
        return delta_decode(gsl::span<const char>(bytes));
    }
};

};  // namespace irkit
