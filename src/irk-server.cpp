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

#include <chrono>
#include <iostream>
#include <sstream>

#include <CLI/CLI.hpp>
#include <CLI/Option.hpp>
#include <boost/filesystem.hpp>

#include <irkit/compacttable.hpp>
#include <irkit/index.hpp>
#include <irkit/index/source.hpp>
#include <irkit/parsing/stemmer.hpp>
#include <irkit/taat.hpp>
#include <irkit/tool/query.hpp>

#include <cpprest/http_listener.h>

using std::uint32_t;
using irk::index::document_t;
using web::http::experimental::listener::http_listener;
using namespace web;
using namespace utility;
using namespace http;

class irk_server {
public:
    irk_server() = default;
    irk_server(utility::string_t url, const irk::inverted_index_view& index)
        : listener_(url), index_(index)
    {
        listener_.support(methods::POST,
            std::bind(&irk_server::handle_post, this, std::placeholders::_1));
    }

    pplx::task<void> open() { return listener_.open(); }
    pplx::task<void> close() { return listener_.close(); }

    void handle_post(http_request message)
    {
        try {
            json::value request = message.extract_json().get();
            std::cout << request << std::endl;
            if (not request.has_field(U("cmd")))
            {
                message.reply(status_codes::NotFound, U("must define 'cmd' field"));
            }
            cmd(request.at(U("cmd")).as_string(), request, message);
        } catch (...) {
            message.reply(status_codes::NotFound, U("unknown error"));
        }
    }

private:
    void cmd(const utility::string_t& name,
        const json::value& data,
        http_request& message)
    {
        if (name == U("query")) { query(data, message); }
        else if (name == U("terminfo")) { terminfo(data, message); }
        else { message.reply(status_codes::NotFound, U("unknown command")); }
    }

    void query(const json::value& data, http_request& message)
    {
        try {
            irk::tool::query cmd;
            cmd.init(data);
            std::stringstream out;
            cmd.execute(out);
            message.reply(status_codes::OK, U(out.str()));
        } catch (const json::json_exception& err) {
            message.reply(status_codes::NotFound, err.what());
        } catch (...) {
            message.reply(status_codes::NotFound, U("unknown error"));
        }
    }

    void terminfo(const json::value& data, http_request& message)
    {
        message.reply(status_codes::OK, U("TERMINFO"));
    }

    http_listener listener_;
    const irk::inverted_index_view& index_;
};

int main(int argc, char** argv)
{
    std::string index_dir = ".";
    std::string port = "34568";

    CLI::App app{"IRKit HTTP inverted index server"};
    app.add_option("-d,--index-dir", index_dir, "index directory", true)
        ->check(CLI::ExistingDirectory);
    app.add_option("-p,--port", port, "as in top-k", true);
    CLI11_PARSE(app, argc, argv);

    std::cerr << "Loading index..." << std::flush;
    boost::filesystem::path dir(index_dir);
    irk::inverted_index_mapped_data_source data(dir, "bm25");
    irk::inverted_index_view index(&data);
    std::cerr << " done." << std::endl;

    utility::string_t address = U("http://localhost:");
    address.append(U(port));
    uri_builder uri(address);
    uri.append_path(U("irk"));

    auto addr = uri.to_uri().to_string();
    irk_server server(addr, index);
    server.open().wait();

    ucout << utility::string_t(U("Listening for requests at: ")) << addr
          << std::endl;
    std::cout << "Press ENTER to exit." << std::endl;

    std::string line;
    std::getline(std::cin, line);

    server.close().wait();
    return 0;

    //std::string index_dir = ".";
    //std::vector<std::string> query;
    //int k = 1000;
    //bool stem = false;
    //int trecid = -1;
    //std::string remap_name;
    //double frac_cutoff;
    //document_t doc_cutoff;


    //if (fraccut->count() > 0u) {
    //    doc_cutoff = static_cast<document_t>(
    //        frac_cutoff * index.titles().size());
    //}

    //if (app.count("--file") == 0u) {
    //    run_query(index,
    //        dir,
    //        query,
    //        k,
    //        stem,
    //        remap_name,
    //        fraccut->count() + idcut->count() > 0u
    //            ? std::make_optional(doc_cutoff)
    //            : std::nullopt,
    //        app.count("--trecid") > 0u ? std::make_optional(trecid)
    //                                   : std::nullopt);
    //}
    //else {
    //    std::optional<int> current_trecid = app.count("--trecid") > 0u
    //        ? std::make_optional(trecid)
    //        : std::nullopt;
    //    for (const auto& file : query) {
    //        std::ifstream in(file);
    //        std::string q;
    //        while(std::getline(in, q))
    //        {
    //            std::istringstream qin(q);
    //            std::string term;
    //            std::vector<std::string> terms;
    //            while (qin >> term) { terms.push_back(std::move(term)); }
    //            run_query(index,
    //                dir,
    //                terms,
    //                k,
    //                stem,
    //                remap_name,
    //                fraccut->count() + idcut->count() > 0u
    //                    ? std::make_optional(doc_cutoff)
    //                    : std::nullopt,
    //                current_trecid);
    //            if (current_trecid.has_value()) { current_trecid.value()++; }
    //        }
    //    }
    //}
}
