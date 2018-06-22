#include <iostream>

#include <CLI/CLI.hpp>
#include <boost/filesystem.hpp>

#include <irkit/compacttable.hpp>

namespace fs = boost::filesystem;

//void run_build(const std::string& input, const std::string& output)
//{
//    auto map = irk::build_prefix_map_from_file<std::size_t>(input);
//    irk::io::dump(map, output);
//}

void run_lookup(const std::string& table_file, int index)
{
    auto table = irk::load_compact_table<std::ptrdiff_t>(table_file);
    if (index >= table.size()) {
        std::cerr << "Given index (" << index << ") is out of range [0-"
                  << table.size() << ")" << std::endl;
        return;
    }
    std::cout << table[index] << std::endl;
}

int main(int argc, char** argv)
{
    std::string input, table_file;
    long index;

    CLI::App app{"Operations related to compact tables."};
    app.require_subcommand(1);

    //CLI::App* build = app.add_subcommand("build", "Build a prefix map");
    //build->add_option("input", input, "input file", false)
    //    ->check(CLI::ExistingFile)
    //    ->required();
    //build->add_option("output", map_file, "output", false)->required();

    CLI::App* lookup = app.add_subcommand(
        "lookup", "Print value at a given position");
    lookup->add_option("table", table_file, "map file", false)
        ->required()
        ->check(CLI::ExistingFile);
    lookup->add_option("index", index, "Table index")
        ->required();

    CLI11_PARSE(app, argc, argv);

    if (*lookup) {
        run_lookup(table_file, index);
        return 0;
    }

    //if (*build) {
    //    run_build(input, map_file);
    //    return 0;
    //}
}
