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

//! \file radix_tree.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <memory>
#include <optional>
#include "irkit/types.hpp"
extern "C" {
#include "rax.h"
}

namespace irk {

template<class T>
class radix_tree {
    // public:
    //    class iterator {
    //    private:
    //        std::shared_ptr<raxIterator> iter_;
    //
    //    public:
    //        iterator(std::shared_ptr<raxIterator> iter) : iter_(iter) {}
    //        ~iterator() { raxStop(iter_.get()); }
    //        bool operator==(const iterator& rhs) const
    //        {
    //
    //        }
    //    };

private:
    rax* c_rax_;
    //std::vector<std::shared_ptr<T>> values_;
    //std::vector<T*> values_;

public:
    radix_tree() { c_rax_ = raxNew(); }
    radix_tree(const radix_tree&) = delete;
    radix_tree(radix_tree&& other) = delete;
    ~radix_tree() { raxFree(c_rax_); }

    int insert(const std::string& key, T value)
    {
        std::vector<char> cstr(key.begin(), key.end());
        T* value_ptr = new T(value);
        void* data = value_ptr;
        int result = raxInsert(c_rax_,
            reinterpret_cast<unsigned char*>(cstr.data()),
            cstr.size(),
            data,
            NULL);
        if (result == 0 && errno == ENOMEM) {
            throw std::bad_alloc();
        }
        return result;
    }

    template<class = enable_if_not_equal<T, void>>
    T operator[](const std::string& key)
    {
        void* data = raxFind(c_rax_,
            reinterpret_cast<const unsigned char*>(key.data()),
            key.size());
        if (data == raxNotFound) {
            throw std::out_of_range("no such element: " + key);
        }
        return *reinterpret_cast<T*>(data);
    }

    template<class = enable_if_not_equal<T, void>>
    std::optional<T> find(const std::string& key)
    {
        void* data = raxFind(c_rax_,
            reinterpret_cast<const unsigned char*>(key.data()),
            key.size());
        if (data == raxNotFound) {
            return std::nullopt;
        }
        return std::make_optional(*reinterpret_cast<T*>(data));
    }

    bool exists(const std::string key)
    {
        void* data = raxFind(c_rax_,
            reinterpret_cast<const unsigned char*>(key.data()),
            key.size());
        return data != raxNotFound;
    }

    std::optional<T> seek_le(std::string key) const
    {
        raxIterator iter;
        raxStart(&iter, c_rax_);
        int result = raxSeek(&iter,
            "<=",
            reinterpret_cast<unsigned char*>(key.data()),
            key.size());
        if (result == 0) {
            throw std::bad_alloc();
        }
        if (raxNext(&iter) == 0) {
            return std::nullopt;
        }
        T val = *reinterpret_cast<T*>(iter.data);
        //std::cout << "seeking: " << key << "; found: " << val << std::endl;
        raxStop(&iter);
        return std::make_optional(val);
    }

    rax* c_rax() const { return c_rax_; }
};

};  // namespace irk
