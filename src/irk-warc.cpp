#include <chrono>
#include <fstream>
#include <iostream>
#include <regex>
#include <stdio.h>

#include <CLI/CLI.hpp>
#include <boost/filesystem.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <gumbo.h>

#include <irkit/io/warc.hpp>
#include <irkit/parsing/html.hpp>
#include <irkit/parsing/snowball/porter2.hpp>

namespace fs = boost::filesystem;

void write_term(std::string&& term, SN_env* z, bool lowercase)
{
    if (lowercase) {
        for (int idx = 0; idx < z->l; ++idx) {
            term[idx] = tolower(term[idx]);
        }
    }
    if (z != nullptr) {
        SN_set_current(
            z, term.size(), reinterpret_cast<unsigned char*>(&term[0]));
        stem(z);
        z->p[z->l] = 0;
        std::cout << z->p;
    } else {
        std::cout << term;
    }
}

std::set<char> parse_field_selection()
{
    std::set<char> selection;
    return selection;
}

class check_fields {
private:
    const std::set<char>& available_fields_;

public:
    check_fields(const std::set<char>& available_fields)
        : available_fields_(available_fields)
    {}
    std::string operator()(const std::string& description)
    {
        if (description.back() != 'b')
        { throw CLI::ValidationError("last field must be body (b)"); }
        for (const char ch : description)
        {
            if (!available_fields_.count(ch))
            {
                std::ostringstream msg;
                msg << "Illegal field requested: " << ch;
                throw CLI::ValidationError(msg.str());
            }
        }
        return std::string();
    }
};

struct body_writer {
    bool lowercase;
    SN_env* z;

    void operator()(const std::string& content)
    {
        std::regex term_pattern("(\\S+)");
        auto term_iter = std::sregex_iterator(
            content.begin(), content.end(), term_pattern);
        if (term_iter != std::sregex_iterator())
        {
            write_term(term_iter->str(), z, lowercase);
            term_iter++;
            for (; term_iter != std::sregex_iterator(); term_iter++)
            {
                std::cout << " ";
                write_term(term_iter->str(), z, lowercase);
            }
            std::cout << std::endl;
        }
    }
};

void print_field(char f,
        const irkit::io::warc_record& record,
        body_writer write_body)
{
    switch (f) {
    case 't':
        std::cout << record.trecid();
        break;
    case 'u':
        std::cout << record.url();
        break;
    case 's':
        std::cout << record.content_length();
        break;
    case 'b':
        std::string content = irk::parsing::html::cleantext(record.content());
        write_body(content);
        break;
    }
}

int main(int argc, char** argv)
{
    std::string field_separator = "\t";
    const std::set<char> available_fields = {'t', 'u', 's', 'b'};
    const std::unordered_map<char, std::string> field_headers = {
        {'t', "title"},
        {'u', "url"},
        {'s', "size"},
        {'b', "body"}
    };
    std::vector<char> fields{'t', 'u', 's', 'b'};
    std::vector<std::string> input_files;

    CLI::App app{"Read and parse WARC collections."};
    app.add_flag("-z,--zip", "use zipped input files");
    app.add_flag("-s,--stem", "stem terms");
    app.add_flag("-l,--lowercase", "transform all characters to lower case");
    app.add_flag("--skip-header", "skip header defining column names");
    app.add_option("-d,--field-delimiter",
        field_separator,
        "field delimiter in the output file",
        true);
    app.add_option("-f,--fields",
        [&fields, available_fields](CLI::results_t vals) {
            fields = std::vector<char>(vals[0].begin(), vals[0].end());
            return true;
        },
        "Fields to output.", true)
        ->expected(1)
        ->check(check_fields(available_fields));
    app.add_option("input", input_files, "input WARC files", false)
        ->required()
        ->check(CLI::ExistingFile);

    CLI11_PARSE(app, argc, argv);

    bool lowercase = app.count("--lowercase");

    SN_env* z = nullptr;
    if (app.count("--stem")) {
        z = create_env();
    }
    body_writer writer{lowercase, z};

    if (!app.count("--skip-header")) {
        std::vector<std::string> headers;
        for (char f : fields) { headers.push_back(field_headers.at(f)); }
        auto it = headers.begin();
        if (it != headers.end()) {
            std::cout << *it++;
            for (; it != headers.end(); it++)
            { std::cout << field_separator << *it; }
            std::cout << "\n";
        }
    }

    for (auto& input_file : input_files) {
        std::ifstream fin(input_file);
        boost::iostreams::filtering_istream in;
        if (app.count("--zip")) {
            in.push(boost::iostreams::gzip_decompressor());
        }
        in.push(fin);

        irkit::io::warc_record record;
        while (irkit::io::read_warc_record(in, record)) {
            if (record.type() == "response") {
                auto it = fields.begin();
                print_field(*it++, record, writer);
                for (; it != fields.end(); it++) {
                    std::cout << field_separator;
                    print_field(*it, record, writer);
                }
            }
        }
    }
    if (app.count("--stem")) {
        close_env(z);
    }
}

