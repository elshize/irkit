#pragma once

#include <algorithm>
#include <bitset>
#include <boost/concept/assert.hpp>
#include <boost/concept_check.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <gsl/span>
#include <vector>
#include "irkit/bitptr.hpp"
#include "irkit/bitstream.hpp"
#include "irkit/coding.hpp"
#include "irkit/coding/varbyte.hpp"

namespace irk::io {

namespace fs = boost::filesystem;

void enforce_exist(const fs::path& file)
{
    if (!fs::exists(file)) {
        throw std::invalid_argument("File not found: " + file.generic_string());
    }
}

void load_data(fs::path data_file, std::vector<char>& data_container)
{
    enforce_exist(data_file);
    std::ifstream in(data_file.c_str(), std::ios::binary);
    in.seekg(0, std::ios::end);
    std::streamsize size = in.tellg();
    in.seekg(0, std::ios::beg);
    data_container.resize(size);
    if (!in.read(data_container.data(), size)) {
        throw std::runtime_error("Failed reading " + data_file.string());
    }
    in.close();
}

//! Appends the underlying bytes of an object to a byte buffer.
template<class T>
void append_object(const T& object, std::vector<char>& buffer)
{
    buffer.insert(buffer.end(),
        reinterpret_cast<const char*>(&object),
        reinterpret_cast<const char*>(&object) + sizeof(object));
}

//! Appends the underlying bytes of a collection to a byte buffer.
template<class Collection>
void append_collection(const Collection& collection, std::vector<char>& buffer)
{
    if (collection.size() > 0) {
        buffer.insert(buffer.end(),
            reinterpret_cast<const char*>(collection.data()),
            reinterpret_cast<const char*>(collection.data())
                + collection.size() * sizeof(collection.front()));
    }
}

};  // namespace irk::io
