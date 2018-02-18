#include <diskhash.hpp>
#include <experimental/filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include "cmd.hpp"
#include "irkit/index.hpp"

namespace fs = std::experimental::filesystem;

int main(int argc, char** argv)
{
    irk::CmdLineProgram program("irk-termmap");
    program.flag("help", "print out help message")
        .option<std::string>("index-dir,d", "index base directory", ".");
    try {
        if (!program.parse(argc, argv)) {
            return 0;
        }
    } catch (irk::po::error& e) {
        std::cout << e.what() << std::endl;
    }

    fs::path dir(program.get<std::string>("index-dir").value());
    fs::path properties_file(irk::index::properties_path(dir));
    fs::path input(dir / "terms.txt");
    fs::path output(dir / "terms.map");

    std::unordered_map<std::string, std::uint32_t> map;

    std::ifstream in(input);
    std::string key;
    std::uint32_t value;
    std::size_t key_maxlen = 0;
    while (std::getline(in, key)) {
        map[key] = value++;
        key_maxlen = std::max(key_maxlen, key.length());
    }
    key_maxlen++;

    fs::remove(output);
    dht::DiskHash<std::uint32_t> ht(output.c_str(), key_maxlen, dht::DHOpenRW);
    for (auto[key, value] : map) {
        ht.insert(key.c_str(), value);
    }

    nlohmann::json properties;
    std::ifstream pin(properties_file);
    pin >> properties;
    pin.close();

    properties["key_maxlen"] = key_maxlen;
    std::ofstream pout(properties_file);
    pout << properties;
    pout.close();
}
