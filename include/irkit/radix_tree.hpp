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

#include <cstring>
#include <memory>
#include <optional>
#include <list>

extern "C" {
#include <rax.h>
}

#include <irkit/types.hpp>

/* Return the current total size of the node. */
#define raxNodeCurrentLength(n) ( \
    sizeof(raxNode)+(n)->size+ \
    ((n)->iscompr ? sizeof(raxNode*) : sizeof(raxNode*)*(n)->size)+ \
    (((n)->iskey && not (n)->isnull) * sizeof(void*)) \
)

namespace irk {


/* Get the node auxiliary data. */
void *raxGetData(raxNode *n) {
    if (n->isnull) { return NULL; }
    void **ndata =(void**)((char*)n+raxNodeCurrentLength(n)-sizeof(void*));
    void *data;
    std::memcpy(&data, ndata, sizeof(data));
    return data;
}

template<class T>
class radix_tree {
private:

    rax* c_rax_;
    std::list<std::unique_ptr<T>> data_;

public:
    radix_tree()
        : c_rax_(raxNew())
    {}
    radix_tree(const radix_tree&) = delete;
    radix_tree(radix_tree&& other) = delete;
    ~radix_tree() { raxFree(c_rax_); }

    int insert(const std::string& key, T value)
    {
        std::vector<char> cstr(key.begin(), key.end());
        data_.push_back(std::make_unique<T>(value));
        void* data = data_.back().get();
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
        std::vector<char> key_data(key.begin(), key.end());
        void* data = raxFind(c_rax_,
            reinterpret_cast<unsigned char*>(key_data.data()),
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
        std::vector<char> key_data(key.begin(), key.end());
        raxIterator iter;
        raxStart(&iter, c_rax_);
        int result = raxSeek(&iter,
            "<=",
            reinterpret_cast<unsigned char*>(key_data.data()),
            key_data.size());
        if (result == 0) {
            throw std::bad_alloc();
        }
        if (raxNext(&iter) == 0) {
            return std::nullopt;
        }
        std::string s((char*)iter.key, iter.key_len);
        T val = *reinterpret_cast<T*>(raxGetData(iter.node));
        raxStop(&iter);
        return std::make_optional(val);
    }

    rax* c_rax() const { return c_rax_; }
};

};  // namespace irk
