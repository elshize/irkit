/**
 * Code relevant to query processing.
 */
#pragma once

#define GSL_UNENFORCED_ON_CONTRACT_VIOLATION

#include <algorithm>
#include <chrono>
#include <iostream>
#include <type_traits>
#include <math.h>
#include "debug_assert.hpp"
#include "heap.hpp"
#include "index.hpp"

namespace bloodhound::query {

template <typename T>
using void_t = void;

template<typename T, typename = void>
struct has_posting_iterator : std::false_type {
};

template<typename T>
struct has_posting_iterator<T, std::enable_if_t<
    std::is_convertible<
        decltype(std::declval<T&>().end()),
        typename T::iterator
    >::value &&
    std::is_convertible<
        decltype(std::declval<T&>().begin()),
        typename T::iterator
    >::value &&
    std::is_convertible<
        decltype(*std::declval<T&>().begin()),
        Posting&
    >::value &&
    std::is_convertible<
        decltype(*std::declval<T&>().end()),
        Posting&
    >::value
>>
    : std::true_type {
};

template<typename T, typename = void>
struct has_iterator : std::false_type {
};

template<typename T>
struct has_iterator<T, void_t<typename T::iterator>> : std::true_type {
};

struct debug : debug_assert::default_handler, debug_assert::set_level<-1> {
};

/// Search result, consisting of the document's ID and score.
/// Any external ID or title is excluded, you must use a title mapping to
/// retrieve it.
struct Result {
    Doc doc;
    Score score;
    Result() = default;
    Result(Doc d, Score s) : doc(d), score(s) {}
    bool operator==(const Result& rhs) const { return score == rhs.score; }
    bool operator<(const Result& rhs) const { return score < rhs.score; }
    bool operator>=(const Result& rhs) const { return score >= rhs.score; }
    bool operator>(const Result& rhs) const { return score > rhs.score; }
};

/// A base abstract super-class of all document retrievers.
template<typename PostingList>
class Retriever {
public:
    /// Retrieves top-k results for the given posting lists and term weights.
    virtual std::vector<Result> retrieve(
        const std::vector<PostingList>& term_postings,
        const std::vector<Score>& term_weights,
        std::size_t k) = 0;
};

/// Converts a min-heap of top-scored documents to a sorted vector of results.
std::vector<Result> heap_to_results(Heap<Score, Doc>& heap)
{
    std::vector<Result> top_results;
    while (!heap.empty()) {
        auto top = heap.pop();
        top_results.emplace_back(top.value, top.key);
    }
    std::reverse(top_results.begin(), top_results.end());
    return top_results;
}

}  // namespace bloodhound::query
