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
#include <regex>
#include <string>
#include <unordered_map>

namespace irkit::io {

using field_map_type = std::unordered_map<std::string, std::string>;

//! Represents an error during parsing WARC format.
class warc_format_exception : public std::exception {
    std::string message_;
    std::string line_;

public:
    warc_format_exception(std::string line, std::string message)
        : message_(std::move(message)), line_(std::move(line))
    {}
    const char* what() const noexcept override
    {
        std::string whatstr = message_ + line_;
        return whatstr.c_str();
    }
    const std::string& line() const { return line_; }
};

class warc_record;
std::istream& read_warc_record(std::istream& in, warc_record& record);

//! A WARC record.
class warc_record {
private:
    std::string version_;
    field_map_type warc_fields_;
    field_map_type http_fields_;
    std::string content_;

public:
    warc_record() = default;
    explicit warc_record(std::string version) : version_(std::move(version)) {}
    std::string type() const { return warc_fields_.at("WARC-Type"); }
    std::string content_length() const
    { return http_fields_.at("Content-Length"); }
    std::string& content() { return content_; }
    const std::string& content() const { return content_; }
    std::string url() const { return warc_fields_.at("WARC-Target-URI"); }
    std::string trecid() const { return warc_fields_.at("WARC-TREC-ID"); }

    friend std::istream&
    read_warc_record(std::istream& in, warc_record& record);
};

namespace warc {

    //! Read WARC record's version.
    inline std::istream& read_version(std::istream& in, std::string& version)
    {
        std::string line;
        std::getline(in, line);
        if (line.empty() && not std::getline(in, line)) {
            return in;
        }
        std::regex version_pattern("^WARC/(.+)$");
        std::smatch sm;
        if (not std::regex_search(line, sm, version_pattern)) {
            throw warc_format_exception(line, "could not parse version: ");
        }
        version = sm.str(1);
        return in;
    }

    //! Read WARC record's fields.
    inline void read_fields(std::istream& in, field_map_type& fields)
    {
        std::string line;
        std::getline(in, line);
        while (not line.empty()) {
            std::regex field_pattern("^(.+):\\s+(.*)$");
            std::smatch sm;
            if (not std::regex_search(line, sm, field_pattern)) {
                //throw WarcFormatException(line, "could not parse field: ");
                std::cerr << "cound not parse field: " << line << std::endl;
            } else {
                fields[sm.str(1)] = sm.str(2);
            }
            std::getline(in, line);
        }
    }

}  // namespace warc

//! Read a WARC record from an input stream.
/*!
    \param in       an input stream
    \param record   a WARC record reference to write to
    \returns        `in` stream
 */
inline std::istream& read_warc_record(std::istream& in, warc_record& record)
{
    std::string version;
    if (not warc::read_version(in, version)) {
        return in;
    }
    record.version_ = std::move(version);
    warc::read_fields(in, record.warc_fields_);
    std::string line;
    if (record.type() != "warcinfo") {
        std::getline(in, line);
    }
    warc::read_fields(in, record.http_fields_);
    if (record.type() != "warcinfo") {
        try {
            std::size_t length = std::stoi(record.content_length());
            record.content_.resize(length);
            in.read(&record.content_[0], length);
            std::getline(in, line);
            std::getline(in, line);
        } catch (std::invalid_argument& error) {
            throw warc_format_exception(
                record.content_length(), "could not parse content length: ");
        }
    }
    return in;
}

}  // namespace irkit::io
