#include <iostream>
#include <memory>
#include <string>
#include <tuple>
#include <vector>
#include "index.hpp"
#include "irkit/daat.hpp"
#include "irkit/taat.hpp"
#include "mongoose.h"
#include "saat.hpp"

using namespace bloodhound;

static index::Index<index::InMemoryPostingPolicy> index_ref;
static std::shared_ptr<query::TaatRetriever<PostingList, false, 0, 0>> taat;
static std::shared_ptr<query::TaatRetriever<PostingList, true, 0, 0>> taat_plus;
static std::shared_ptr<query::RawTaatRetriever<PostingList>> raw_taat;
static std::shared_ptr<query::WandRetriever<PostingList>> wand;
static std::shared_ptr<query::MaxScoreRetriever<PostingList>> mscore;
static std::shared_ptr<query::TaatMaxScoreRetriever<PostingList>> tmscore;
static std::shared_ptr<query::ExactSaatRetriever<PostingList>> saat;
static std::shared_ptr<query::MaxScoreNonEssentials<PostingList>> ness;
static std::shared_ptr<query::ThresholdRetriever<PostingList>> ta;
static std::shared_ptr<index::Index<>> index_ptr;

std::tuple<std::vector<PostingList>, std::vector<Score>> parse_query(
    const std::string& query_line)
{
    std::vector<std::tuple<TermId, Score>> query;
    std::string term_pair;
    std::istringstream line_stream(query_line);
    while (line_stream >> term_pair) {
        std::size_t pos = term_pair.find(":");
        term_pair[pos] = ' ';
        std::istringstream pair_stream(term_pair);
        TermId termid;
        Score score;
        pair_stream >> termid;
        pair_stream >> score;
        query.push_back(std::make_tuple(termid, score));
    }
    std::vector<PostingList> query_posting_lists;
    std::vector<Score> term_weights;
    for (const auto & [termid, weight] : query) {
        if (weight == Score(0))
            continue;
        auto posting_list = index_ptr->posting_list(termid);
        query_posting_lists.push_back(posting_list);
        term_weights.push_back(weight);
    }
    return std::make_tuple(query_posting_lists, term_weights);
}

template<class Posting>
std::vector<query::Result> to_results(std::vector<Posting> postings)
{
    std::vector<query::Result> results;
    for (auto& [doc, score] : postings) {
        results.push_back({doc, score});
    }
    return results;
}

std::vector<query::Result> daat(std::vector<PostingList> postings,
    std::vector<Score> weights,
    std::size_t k)
{
    auto r = irkit::daat_or(postings, k, weights);
    std::vector<query::Result> results;
    for (auto& [doc, score] : r) {
        results.push_back({doc, score});
    }
    return results;
}

static void ev_handler(struct mg_connection* connection, int ev, void* p)
{
    if (ev == MG_EV_HTTP_REQUEST) {
        // Parse request JSON
        auto request = static_cast<http_message*>(p);
        auto request_body = std::string(request->body.p, request->body.len);
        json::json params = json::json::parse(request_body);
        auto[posting_lists, weights] = parse_query(params["query"]);

        json::json response = {};

        auto start_interval = std::chrono::steady_clock::now();
        std::vector<query::Result> results;
        json::json stats = {};
        if (params["type"] == "taat") {
            results = taat->retrieve(posting_lists, weights, params["k"]);
            stats = taat->stats();
        } else if (params["type"] == "rtaat") {
            results = raw_taat->retrieve(posting_lists, weights, params["k"]);
            stats = raw_taat->stats();
        } else if (params["type"] == "taat+") {
            results = taat_plus->retrieve(posting_lists, weights, params["k"]);
            stats = taat_plus->stats();
        } else if (params["type"] == "daat") {
            results =
                to_results(irkit::daat_or(posting_lists, params["k"], weights));
        } else if (params["type"] == "wand") {
            results =
                to_results(irkit::wand(posting_lists, params["k"], weights));
        } else if (params["type"] == "mscore") {
            results = mscore->retrieve(posting_lists, weights, params["k"]);
            stats = mscore->stats();
        } else if (params["type"] == "tmscore") {
            results = tmscore->retrieve(posting_lists, weights, params["k"]);
            stats = tmscore->stats();
        } else if (params["type"] == "saat") {
            double et = 1.0;
            try {
                et = params.at("saat_et_threshold").get<double>();
            } catch (...) {}
            saat->set_et_threshold(et);
            results = saat->retrieve(posting_lists, weights, params["k"]);
            //stats = saat->stats();
        } else if (params["type"] == "asaat") {
            double et = 1.0;
            try {
                et = params.at("saat_et_threshold").get<double>();
            } catch (...) {}
            for (auto& postlist : posting_lists) {
                postlist.make_et(et);
            }
            results = taat->retrieve(posting_lists, weights, params["k"]);
            stats = taat->stats();
        } else if (params["type"] == "ness") {
            results = ness->retrieve(posting_lists, weights, params["k"]);
            stats = ness->stats();
        } else if (params["type"] == "ta") {
            results = ta->retrieve(posting_lists, weights, params["k"]);
            stats = ta->stats();
        }
        auto end_interval = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            end_interval - start_interval);

        std::vector<json::json> result_list;
        for (const auto& result : results) {
            result_list.push_back({{"doc", type_safe::get(result.doc)},
                {"score", type_safe::get(result.score)}});
        }
        response["results"] = result_list;
        response["nanoseconds"] = elapsed.count();
        response["stats"] = stats;
        std::string response_text = response.dump();
        std::cerr << response_text << std::endl;

        mg_printf(connection,
            "%s",
            "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
        mg_printf_http_chunk(connection, response.dump().c_str());
        mg_send_http_chunk(
            connection, "", 0); /* Send empty chunk, the end of response */
    }
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "usage: server <index_dir> [<port>]\n";
        return 1;
    }
    std::string index_dir(argv[1]);
    std::string port("8000");
    if (argc > 2)
        port = argv[2];

    std::cerr << "Loading index located at: " << index_dir << std::endl;
    //index::Index<>::write_maxscores(index_dir);
    index::Index idx = index::Index<>::load_index(index_dir);
    index_ptr.reset(&idx);

    taat.reset(new query::TaatRetriever<PostingList>(
        index_ptr->get_collection_size()));
    raw_taat.reset(new query::RawTaatRetriever<PostingList>(
        index_ptr->get_collection_size()));
    //daat.reset(new query::DaatRetriever<PostingList>());
    wand.reset(new query::WandRetriever<PostingList>());
    mscore.reset(new query::MaxScoreRetriever<PostingList>());
    tmscore.reset(new query::TaatMaxScoreRetriever<PostingList>(
        index_ptr->get_collection_size()));
    saat.reset(new query::ExactSaatRetriever<PostingList>(
                index_ptr->get_collection_size()));
    ness.reset(new query::MaxScoreNonEssentials<PostingList>(
        index_ptr->get_collection_size()));
    ta.reset(new query::ThresholdRetriever<PostingList>(
        index_ptr->get_collection_size()));

    struct mg_mgr mgr;
    mg_mgr_init(&mgr, NULL);
    mg_connection* connection = mg_bind(&mgr, port.c_str(), ev_handler);
    if (connection == NULL) {
        std::cerr << "Failed to create listener\n";
        return 1;
    }
    std::cerr << "Bloodhound running at port " << port << std::endl;

    // Set up HTTP server parameters
    mg_set_protocol_http_websocket(connection);

    for (;;) {
        try {
            mg_mgr_poll(&mgr, 1000);
        } catch (const nlohmann::detail::parse_error& er) {
            std::cerr << "ERROR: " << er.what() << std::endl;
        }
    }
    mg_mgr_free(&mgr);

    return 0;
}
