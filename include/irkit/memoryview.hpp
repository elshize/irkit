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

//! \file memoryview.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <boost/filesystem.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <gsl/gsl_util>
#include <gsl/span>
#include <iostream>
#include <vector>

namespace fs = boost::filesystem;

namespace irk {

template<class T>
struct reinterpret_cast_fn {
    T operator()(const char* pos) const
    {
        return *reinterpret_cast<const T*>(pos);
    }
};

//! A base class for all types of memory views.
/*!
 * A memory view is an abstraction for accessing any memory area, be it in
 * main memory, or on disk, or via a memory mapped file.
 * The constructor accepts different types of memory sources that implement
 * memory access details. These types are hidden with type erasure: only
 * the constructor is a template, and `memory_view` type can be used
 * polymorphically.
 */
class memory_view {
public:
    using slice_end = std::optional<int>;
    using slice_type = std::pair<slice_end, slice_end>;

    //! Creates a memory view.
    /*!
     * \tparam source_type  a memory source type
     * \param  source       a copy of this will be created on heap to support
     *                      polymorphic usage with value semantic.
     */
    template<class source_type>
    explicit memory_view(source_type source)
        : self_(std::make_shared<model<source_type>>(source))
    {}

    memory_view(const memory_view& other) = default;

    //! Returns a pointer to the underlying data.
    /*!
     * The source is expected to return a pointer of contiguous data in
     * memory (or memory mapped file). Thus, if a source loads data lazily,
     * it must first load the entire data and return its pointer.
     * To avoid such inefficiency, e.g., if data is meant to be accessed
     * sequentially, use \ref begin() and \ref end(), or \ref operator[]
     * instead.
     */
    const char* data() const { return self_->data(); }

    //! Returns the number of characters in the memory area.
    gsl::index size() const { return self_->size(); }

    //! Returns a  a character at offset `n` in the memory.
    const char& operator[](int n) const { return (*self_)[n]; }

    //! Returns a new memory view defined by the given range.
    irk::memory_view range(int first, int size) const
    {
        return {self_->range(first, size)};
    }

    //! Returns a new memory view defined by the given slice.
    irk::memory_view operator[](slice_type slice) const
    {
        int left = slice.first.value_or(0);
        int right = slice.second.value_or(size() - 1);
        assert(left <= right);
        return range(left, right - left + 1);
    }

    template<class T, class CastFn = reinterpret_cast_fn<T>>
    T as(CastFn cast_fn = reinterpret_cast_fn<T>{}) const
    {
        return cast_fn(data());
    }

    //! Character iterator.
    class iterator : public boost::iterator_facade<
        iterator, const char, boost::random_access_traversal_tag> {
    public:
        iterator() = default;
        iterator(const char* pos) : pos_(pos) {}

    private:
        friend class boost::iterator_core_access;
        void increment() { pos_++; }
        void decrement() { pos_--; }
        void advance(difference_type n) { pos_ += n; }
        difference_type distance_to(const iterator& other) const
        {
            return other.pos_ - pos_;
        }
        bool equal(const iterator& other) const { return pos_ == other.pos_; }
        const char& dereference() const { return *pos_; }
        const char* pos_;
    };

    iterator begin() const { return {data()}; }
    iterator end() const { return {data() + size()}; }

    struct concept
    {
        virtual ~concept() = default;
        virtual const char* data() const = 0;
        virtual gsl::index size() const = 0;
        virtual const char& operator[](int) const = 0;
        virtual memory_view range(int first, gsl::index size) const = 0;
    };

    template<class source_type>
    class model : public concept {
    public:
        model(source_type source) : source_(source) {}
        const char* data() const override { return source_.data(); }
        gsl::index size() const override { return source_.size(); }
        const char& operator[](int n) const override { return source_[n]; }
        memory_view range(int first, gsl::index size) const override
        {
            return memory_view(source_.range(first, size));
        }

    private:
        source_type source_;
    };

private:
    std::shared_ptr<concept> self_;
};

//! A gsl::span<T> memory source.
template<class CharT = char>
class span_memory_source {
public:
    using char_type = CharT;
    using pointer = char_type*;
    using reference = char_type&;
    span_memory_source() = default;
    span_memory_source(gsl::span<const char_type> memory_span)
        : span_(memory_span){};
    const char_type* data() const { return span_.data(); }
    gsl::index size() const { return span_.size(); }
    const char_type& operator[](int n) const { return span_[n]; }
    span_memory_source range(int first, gsl::index size) const
    {
        return span_memory_source(span_.subspan(first, size));
    }

private:
    gsl::span<const char_type> span_;
};

//! A pointer memory source.
template<class CharT = char>
class pointer_memory_source {
public:
    using char_type = CharT;
    using pointer = char_type*;
    using reference = char_type&;
    pointer_memory_source() = default;
    pointer_memory_source(const char_type* data, gsl::index size)
        : data_(data), size_(size){};
    const char_type* data() const { return data_; }
    gsl::index size() const { return size_; }
    const char_type& operator[](int n) const { return data_[n]; }
    pointer_memory_source range(int first, gsl::index size) const
    {
        return pointer_memory_source(data_ + first, size);
    }

private:
    const char_type* data_;
    gsl::index size_;
};


memory_view make_memory_view(gsl::span<const char> mem)
{
    return memory_view(span_memory_source(mem));
}

memory_view make_memory_view(const char* data, int size)
{
    return memory_view(pointer_memory_source(data, size));
}

//! A memory source for mapped files.
template<class CharT = char>
class mapped_file_memory_source {
public:
    using char_type = CharT;
    using pointer = char_type*;
    using reference = char_type&;
    mapped_file_memory_source() = default;
    mapped_file_memory_source(boost::iostreams::mapped_file_source file)
        : file_(file) {}
    const char_type* data() const { return file_.data(); }
    gsl::index size() const { return file_.size(); }
    char operator[](int n) const { return data()[n]; }
    pointer_memory_source<char_type> range(int first, gsl::index size) const
    {
        return pointer_memory_source(file_.data() + first, size);
    }

private:
    boost::iostreams::mapped_file_source file_;
};

};  // namespace irk
