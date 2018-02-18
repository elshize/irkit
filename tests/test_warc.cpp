#include <vector>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#define private public
#define protected public
#include "irkit/io/warc.hpp"

namespace {

class WarcRecord : public ::testing::Test {
protected:
    std::string warcinfo =
        "WARC/0.18\n"
        "WARC-Type: warcinfo\n"
        "WARC-Date: 2009-03-65T08:43:19-0800\n"
        "WARC-Record-ID: <urn:uuid:993d3969-9643-4934-b1c6-68d4dbe55b83>\n"
        "Content-Type: application/warc-fields\n"
        "Content-Length: 219\n"
        "\n"
        "software: Nutch 1.0-dev (modified for clueweb09)\n"
        "isPartOf: clueweb09-en\n"
        "description: clueweb09 crawl with WARC output\n"
        "format: WARC file version 0.18\n"
        "conformsTo: "
        "http://www.archive.org/documents/WarcFileFormat-0.18.html\n"
        "\n";
    std::string response =
        "WARC/0.18\n"
        "WARC-Type: response\n"
        "WARC-Target-URI: http://00000-nrt-realestate.homepagestartup.com/\n"
        "WARC-Warcinfo-ID: 993d3969-9643-4934-b1c6-68d4dbe55b83\n"
        "WARC-Date: 2009-03-65T08:43:19-0800\n"
        "WARC-Record-ID: <urn:uuid:67f7cabd-146c-41cf-bd01-04f5fa7d5229>\n"
        "WARC-TREC-ID: clueweb09-en0000-00-00000\n"
        "Content-Type: application/http;msgtype=response\n"
        "WARC-Identified-Payload-Type: \n"
        "Content-Length: 16558\n"
        "\n"
        "HTTP/1.1 200 OK\n"
        "Content-Type: text/html\n"
        "Date: Tue, 13 Jan 2009 18:05:10 GMT\n"
        "Pragma: no-cache\n"
        "Cache-Control: no-cache, must-revalidate\n"
        "X-Powered-By: PHP/4.4.8\n"
        "Server: WebServerX\n"
        "Connection: close\n"
        "Last-Modified: Tue, 13 Jan 2009 18:05:10 GMT\n"
        "Expires: Mon, 20 Dec 1998 01:00:00 GMT\n"
        "Content-Length: 10\n"
        "\n"
        "Content...";
};

TEST(WarcVersion, valid)
{
    std::istringstream in("WARC/0.18\nUnrelated text");
    std::string version;
    irkit::io::warc::read_version(in, version);
    EXPECT_EQ(version, "0.18");
}

TEST(WarcVersion, invalid)
{
    std::istringstream in("INVALID_STRING");
    std::string version;
    ASSERT_THROW(irkit::io::warc::read_version(in, version),
        irkit::io::warc_format_exception);
}

TEST(WarcVersion, new_line)
{
    std::istringstream in("\n");
    std::string version = "initial";
    EXPECT_EQ(irkit::io::warc::read_version(in, version).eof(), true);
    EXPECT_EQ(version, "initial");

}

TEST(WarcFields, valid)
{
    std::istringstream in(
        "WARC-Type: warcinfo\n"
        "Content-Type: application/warc-fields\n"
        "Content-Length: 219\n"
        "\n");
    irkit::io::field_map_type fields;
    irkit::io::warc::read_fields(in, fields);
    EXPECT_EQ(fields["WARC-Type"], "warcinfo");
    EXPECT_EQ(fields["Content-Type"], "application/warc-fields");
    EXPECT_EQ(fields["Content-Length"], "219");
}

TEST(WarcFields, invalid)
{
    std::istringstream in(
        "WARC-Type warcinfo\n"
        "\n");
    irkit::io::field_map_type fields;
    irkit::io::warc::read_fields(in, fields);
    EXPECT_EQ(fields["WARC-Type"], "");
}

TEST_F(WarcRecord, warcinfo)
{
    std::istringstream in(warcinfo);
    irkit::io::warc_record record;
    irkit::io::read_warc_record(in, record);
    std::string line;
    std::getline(in, line);
    EXPECT_EQ(line, "");
    EXPECT_EQ(record.http_fields_["conformsTo"],
        "http://www.archive.org/documents/WarcFileFormat-0.18.html");
}

TEST_F(WarcRecord, response)
{
    std::istringstream in(response);
    irkit::io::warc_record record;
    irkit::io::read_warc_record(in, record);
    std::string line;
    std::getline(in, line);
    EXPECT_EQ(line, "");
    EXPECT_EQ(record.type(), "response");
    EXPECT_EQ(record.content(), "Content...");
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

