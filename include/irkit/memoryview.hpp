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

#include <iostream>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <gsl/gsl_util>
#include <gsl/span>

#include <irkit/assert.hpp>
#include <irkit/io.hpp>

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
    using slice_end = std::optional<std::ptrdiff_t>;
    using slice_type = std::pair<slice_end, slice_end>;
    using iterator = const char*;

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

    memory_view() = default;
    memory_view(const memory_view&) = default;
    memory_view(memory_view&& other) = default;
    memory_view& operator=(const memory_view&) = default;
    memory_view& operator=(memory_view&&) noexcept = default;
    ~memory_view() = default;

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
    std::ptrdiff_t size() const { return self_->size(); }

    //! Returns a character at offset `n` in the memory.
    const char& operator[](std::ptrdiff_t n) const { return (*self_)[n]; }

    //! Returns a new memory view defined by the given range.
    irk::memory_view range(std::ptrdiff_t first, std::ptrdiff_t size) const
    {
        return {self_->range(first, size)};
    }

    //! Returns a new memory view defined by the given slice.
    irk::memory_view operator[](slice_type slice) const
    {
        std::ptrdiff_t left = slice.first.value_or(0);
        std::ptrdiff_t right = slice.second.value_or(size() - 1);
        EXPECTS(left <= right);
        return range(left, right - left + 1);
    }

    //! Returns a new memory view defined by a half-open range [lo, hi).
    irk::memory_view operator()(std::ptrdiff_t lo, std::ptrdiff_t hi) const
    {
        EXPECTS(lo < hi);
        return range(lo, hi - lo);
    }

    //! Returns a new memory view defined by the given cut.
    irk::memory_view operator()(std::ptrdiff_t cut) const
    {
        if (cut < 0) { return range(size() + cut, -cut); }
        return range(0, cut);
    }

    template<class T, class CastFn = reinterpret_cast_fn<T>>
    T as(CastFn cast_fn = reinterpret_cast_fn<T>{}) const
    {
        return cast_fn(data());
    }


    iterator begin() const { return data(); }
    iterator end() const { return data() + size(); }

    auto stream() const
    {
        return boost::iostreams::stream<
            boost::iostreams::basic_array_source<char>>(data(), size());
    }

    struct concept
    {
        concept() = default;
        concept(const concept&) = default;
        concept(concept&&) noexcept = default;
        concept& operator=(const concept&) = default;
        concept& operator=(concept&&) noexcept = default;
        virtual ~concept() = default;
        virtual const char* data() const = 0;
        virtual gsl::index size() const = 0;
        virtual const char& operator[](std::ptrdiff_t) const = 0;
        virtual memory_view range(std::ptrdiff_t first, std::ptrdiff_t size)
            const = 0;
    };

    template<class source_type>
    class model : public concept {
    public:
        explicit model(source_type source) : source_(std::move(source)) {}
        const char* data() const override { return source_.data(); }
        gsl::index size() const override { return source_.size(); }

        const char& operator[](std::ptrdiff_t n) const override
        { return source_[n]; }

        memory_view
        range(std::ptrdiff_t first, std::ptrdiff_t size) const override
        {
            return memory_view(source_.range(first, size));
        }

    private:
        source_type source_;
    };

private:
    std::shared_ptr<concept> self_;
};

inline std::ostream& operator<<(std::ostream& out, const memory_view& mv)
{
    for (const char& byte : mv) { out << static_cast<int>(byte) << " "; }
    return out;
}

//! A gsl::span<T> memory source.
template<class CharT = char>
class span_memory_source {
public:
    using char_type = CharT;
    using pointer = char_type*;
    using reference = char_type&;
    span_memory_source() = default;
    explicit span_memory_source(gsl::span<const char_type> memory_span)
        : span_(memory_span){};
    const char_type* data() const { return span_.data(); }
    std::ptrdiff_t size() const { return span_.size(); }
    const char_type& operator[](std::ptrdiff_t n) const { return span_[n]; }
    span_memory_source range(std::ptrdiff_t first, std::ptrdiff_t size) const
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
    pointer_memory_source(const pointer_memory_source&) = default;
    pointer_memory_source(pointer_memory_source&&) noexcept = default;
    pointer_memory_source& operator=(const pointer_memory_source&) = default;
    pointer_memory_source&
    operator=(pointer_memory_source&&) noexcept = default;
    pointer_memory_source(const char_type* data, std::ptrdiff_t size)
        : data_(data), size_(size){};
    ~pointer_memory_source() = default;
    const char_type* data() const { return data_; }
    std::ptrdiff_t size() const { return size_; }
    const char_type& operator[](std::ptrdiff_t n) const { return data_[n]; }
    pointer_memory_source range(std::ptrdiff_t first, std::ptrdiff_t size) const
    {
        return pointer_memory_source(data_ + first, size);
    }

private:
    const char_type* data_ = nullptr;
    std::ptrdiff_t size_ = 0;
};

inline memory_view make_memory_view(const std::vector<char>& mem)
{
    return memory_view(pointer_memory_source(mem.data(), mem.size()));
}

inline memory_view make_memory_view(gsl::span<const char> mem)
{
    return memory_view(span_memory_source<const char>(mem));
}

inline memory_view make_memory_view(const char* data, std::ptrdiff_t size)
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
    explicit mapped_file_memory_source(
        const boost::iostreams::mapped_file_source& file)
        : file_(file)
    {}
    const char_type* data() const { return file_.data(); }
    std::ptrdiff_t size() const { return file_.size(); }
    char operator[](std::ptrdiff_t n) const { return data()[n]; }

    pointer_memory_source<char_type>
    range(std::ptrdiff_t first, std::ptrdiff_t size) const
    {
        return pointer_memory_source(file_.data() + first, size);
    }

private:
    const boost::iostreams::mapped_file_source& file_;
};

//! A memory source that loads data from disk.
template<class CharT = char>
class disk_memory_source {
public:
    using char_type = CharT;
    using pointer = char_type*;
    using reference = char_type&;

    disk_memory_source() = default;
    disk_memory_source(const disk_memory_source&) = default;

    disk_memory_source& operator=(const disk_memory_source&) = default;
    ~disk_memory_source() noexcept = default;

    disk_memory_source(disk_memory_source&&) noexcept = default;
    disk_memory_source& operator=(disk_memory_source&&) noexcept = default;

    explicit disk_memory_source(boost::filesystem::path file_path)
        : file_path_(std::move(file_path)), size_(calc_size())
    {}
    disk_memory_source(boost::filesystem::path file_path,
        std::streamsize offset,
        std::streamsize size)
        : file_path_(std::move(file_path)), offset_(offset), size_(size)
    {}

    const char_type* data() const
    {
        ensure_loaded();
        return &buffer_[0];
    }

    std::streamsize size() const { return size_; }

    const char_type& operator[](std::ptrdiff_t n) const { return data()[n]; }

    disk_memory_source<char_type>
    range(std::streamsize first, std::streamsize size) const
    {
        return disk_memory_source(file_path_, offset_ + first, size);
    }

private:
    boost::filesystem::path file_path_{};
    std::streamsize offset_ = 0;
    std::streamsize size_ = 0;
    mutable std::vector<char_type> buffer_;

    void ensure_loaded() const
    {
        if (buffer_.empty()) {
            io::enforce_exist(file_path_);
            auto flags = std::ifstream::ate | std::ifstream::binary;
            std::ifstream in(file_path_.c_str(), flags);
            in.seekg(offset_, std::ios::beg);
            buffer_.resize(size_);
            if (not in.read(buffer_.data(), size_))
            {
                throw std::runtime_error(
                    "Failed reading " + file_path_.string());
            }
        }
    }

    std::streamsize calc_size() const
    {
        auto flags = std::ifstream::ate | std::ifstream::binary;
        std::ifstream in(file_path_.c_str(), flags);
        return in.tellg();
    }
};

inline memory_view make_memory_view(const boost::filesystem::path& file_path)
{
    return memory_view(disk_memory_source(file_path));
}

};  // namespace irk
