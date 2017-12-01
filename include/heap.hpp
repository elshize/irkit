#pragma once

#include <functional>
#include <gsl/span>
#include <iostream>
#include <string>
#include <vector>

namespace bloodhound {

/// A vector-based heap priority queue; a min-heap by default.
///
/// This implementation provides an implementation of an atomic pull-push
/// operation, which is faster than a pull followed by push.
/// Additionally, it forces you to always indicate the key and the value;
/// it makes it easier to declare heaps without a need to create separate
/// structures sorted by the key alone: all the heavy lifting is implemented
/// within the Heap::Entry type.
///
/// @tparam Key         the type of the key
/// @tparam Value       the type of the value
/// @tparam Compare     a compare type providing a weak ordering
template<class Key, class Value, class Compare = std::less<Key>>
class Heap {

public:
    /// The type of objects stored internally in the heap.
    struct Entry {
        Key key;
        Value value;
        Entry() = default;
        Entry(Key key, Value value) : key(key), value(value) {}
        bool operator==(const Entry& rhs) const
        {
            return key == rhs.key && value == rhs.value;
        }
        bool operator<(const Entry& rhs) const { return key < rhs.key; }
        bool operator>(const Entry& rhs) const { return key > rhs.key; }
        bool operator<=(const Entry& rhs) const { return key <= rhs.key; }
        bool operator>=(const Entry& rhs) const { return key >= rhs.key; }
        friend std::ostream& operator<<(std::ostream& os, Entry& he)
        {
            return os << "(" << he.key << "->" << he.value << ")";
        }
    };

    /// Heap constructor.
    ///
    /// @param capacity     the initial capacity of the internal vector;
    ///                     use whenever the (max) size is known beforehand
    Heap(std::size_t capacity = 0) : container(1), compare(Compare())
    {
        container.reserve(capacity + 1);
    }

    std::size_t size() { return container.size() - 1; }
    bool empty() { return container.size() == 1; }

    /// Returns a copy of the top element; does not modify the heap.
    Entry top() { return container[1]; }

    /// Adds a new element to the heap.
    void push(Key key, Value value)
    {
        container.emplace_back(key, value);
        heapify_up(size());
    }

    /// Adds a new element to the heap unless the capacity limit is reached;
    /// in case of an overflow, the new element replaces the top element only if
    /// it is not smaller (for min-heap) than the current top key.
    void push_with_limit(Key key, Value value, std::size_t limit)
    {
        if (size() < limit)
            push(key, value);
        else if (!compare(key, top().key))
            pop_push(key, value);
    }

    /// Replaces the top element with a given value and returns the former.
    Entry pop_push(Key key, Value value)
    {
        Entry popped = container[1];
        container[1] = {key, value};
        heapify_down();
        return popped;
    }

    /// Returns the top element and removes it from the heap.
    Entry pop()
    {
        Entry popped = container[1];
        container[1] = container[size()];
        container.pop_back();
        heapify_down();
        return popped;
    }

    typename std::vector<Entry>::iterator begin()
    {
        return container.begin() + 1;
    }

    typename std::vector<Entry>::iterator end() { return container.end(); }

private:
    std::vector<Entry> container;
    Compare compare;

    void heapify_up(std::size_t at)
    {
        std::size_t parent;
        while (at > 1
            && compare(container[at].key, container[(parent = at / 2)].key)) {
            std::swap(container[at], container[parent]);
            at = parent;
        }
    }

    void heapify_down(std::size_t at = 1)
    {
        std::size_t left, right;
        while ((left = 2 * at) <= size()) {
            right = left + 1;
            std::size_t smaller_child = right > size()
                ? left
                : compare(container[left].key, container[right].key) ? left
                                                                     : right;
            if (!compare(container[smaller_child].key, container[at].key)) {
                break;
            }
            std::swap(container[at], container[smaller_child]);
            at = smaller_child;
        }
    }
};

template<class Key, class Value>
typename Heap<Key, Value>::Entry make_entry(Key key, Value value)
{
    typename Heap<Key, Value>::Entry entry(key, value);
    return entry;
}

};  // namespace bloodhound
