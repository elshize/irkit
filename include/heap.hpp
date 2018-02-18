#pragma once

#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace irk {

template<typename T>
using void_t = void;

template<typename T, typename = void>
struct has_default_constructor : std::false_type {
};

template<typename T>
struct has_default_constructor<T, void_t<decltype(std::declval<T&>())>>
    : std::true_type {
};

class EmptyMapping {
};

/// The type of objects stored internally in the heap.
template<class Key, class Value>
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

/// A vector-based heap priority queue; a min-heap by default.
///
/// This implementation provides an implementation of an atomic pull-push
/// operation, which is faster than a pull followed by push.
/// Additionally, it forces you to always indicate the key and the value;
/// it makes it easier to declare heaps without a need to create separate
/// structures sorted by the key alone: all the heavy lifting is implemented
/// within the Entry type.
///
/// @tparam Key         the type of the key
/// @tparam Value       the type of the value
/// @tparam Compare     a compare type providing a weak ordering
/// @tparam Mapping     a type of mapping between keys and their position;
///                     empty by default; if empty, *_by_key() operations are
///                     unavailable
template<class Key,
    class Value,
    class Compare = std::less<Key>,
    class Mapping = EmptyMapping>
class Heap {

public:
    /// Heap constructor.
    ///
    /// @param capacity     the initial capacity of the internal vector;
    ///                     use whenever the (max) size is known beforehand
    Heap(std::size_t capacity = 0) : container(1), compare(Compare())
    {
        container.reserve(capacity + 1);
    }

    std::size_t size() const { return container.size() - 1; }
    bool empty() const { return container.size() == 1; }

    /// Returns a copy of the top element; does not modify the heap.
    Entry<Key, Value> top() const { return container[1]; }

    /// Adds a new element to the heap.
    void push(Key key, Value value)
    {
        if constexpr (!std::is_same<Mapping, EmptyMapping>::value) {
            auto pos = mapping[value];
            if (pos != 0) {
                container[pos] = {key, value};
                heapify_either(pos);
            } else {
                container.emplace_back(key, value);
                mapping[value] = size();
                heapify_up(size());
            }
        } else {
            container.emplace_back(key, value);
            heapify_up(size());
        }
    }

    /// Adds a new element to the heap unless the capacity limit is reached;
    /// in case of an overflow, the new element replaces the top element only if
    /// it is not smaller (for min-heap) than the current top key.
    void push_with_limit(Key key, Value value, std::size_t limit)
    {
        if constexpr (!std::is_same<Mapping, EmptyMapping>::value) {
            auto pos = mapping[value];
            if (pos != 0) {
                container[pos] = {key, value};
                heapify_either(pos);
                return;
            }
        }
        if (size() < limit) {
            push(key, value);
        } else if (!compare(key, top().key)) {
            pop_push(key, value);
        }
    }

    /// Replaces the top element with a given value and returns the former.
    Entry<Key, Value> pop_push(Key key, Value value)
    {
        if constexpr (!std::is_same<Mapping, EmptyMapping>::value) {
            if (mapping[value] != 0) {
                throw std::invalid_argument(
                    "cannot pop_push value that is already in heap but is not "
                    "min");
            }
        }
        Entry popped = container[1];
        container[1] = {key, value};
        if constexpr (!std::is_same<Mapping, EmptyMapping>::value) {
            mapping[popped.value] = 0;
            mapping[value] = 1;
        }
        heapify_down();
        return popped;
    }

    /// Returns the top element and removes it from the heap.
    Entry<Key, Value> pop()
    {
        Entry popped = container[1];
        container[1] = container[size()];
        if constexpr (!std::is_same<Mapping, EmptyMapping>::value) {
            mapping[popped.value] = 0;
        }
        container.pop_back();
        heapify_down();
        return popped;
    }

    void remove_value(Value value)
    {
        std::size_t position = mapping[value];
        if (position > 0) {
            remove_at(position);
        }
    }

    typename std::vector<Entry<Key, Value>>::iterator begin()
    {
        return container.begin() + 1;
    }

    typename std::vector<Entry<Key, Value>>::iterator end()
    {
        return container.end();
    }

    void clear()
    {
        container.clear();
        container.resize(1);
        if constexpr (!std::is_same<Mapping, EmptyMapping>::value) {
            mapping.clear();
        }
    }

    Key key_or(Value value, Key def)
    {
        return mapping[value] == 0 ? def : container[mapping[value]].key;
    }

private:
    Mapping mapping;
    std::vector<Entry<Key, Value>> container;
    Compare compare;

    void heapify_up(std::size_t at)
    {
        std::size_t parent;
        while (at > 1
            && compare(container[at].key, container[(parent = at / 2)].key)) {
            if constexpr (!std::is_same<Mapping, EmptyMapping>::value) {
                mapping[container[at].value] = parent;
                mapping[container[parent].value] = at;
            }
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
            if constexpr (!std::is_same<Mapping, EmptyMapping>::value) {
                mapping[container[at].value] = smaller_child;
                mapping[container[smaller_child].value] = at;
            }
            std::swap(container[at], container[smaller_child]);
            at = smaller_child;
        }
    }

    void heapify_either(std::size_t position)
    {
        std::size_t parent = position / 2;
        if (compare(container[position].key, container[parent].key)) {
            heapify_up(position);
        } else {
            heapify_down(position);
        }
    }

    void remove_at(std::size_t position)
    {
        if constexpr (!std::is_same<Mapping, EmptyMapping>::value) {
            mapping[container[position].value] = 0;
            mapping[container[size()].value] = position;
        }
        std::swap(container[position], container[size()]);
        container.pop_back();
        heapify_either(position);
    }
};

template<class Key, class Value>
Entry<Key, Value> make_entry(Key key, Value value)
{
    Entry<Key, Value> entry(key, value);
    return entry;
}

};  // namespace irk
