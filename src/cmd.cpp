#include "cmd.hpp"
#include <iostream>

namespace irk {

void print_help(std::string program,
    const po::options_description& flags,
    const po::options_description& positional,
    const po::positional_options_description& positions)
{
    std::cout << "Usage: " << program << " [options]";
    bool infinite =
        positions.max_total_count() == std::numeric_limits<unsigned>::max();
    for (std::size_t idx = 0; idx < positions.max_total_count(); ++idx) {
        std::string name = positions.name_for_position(idx);
        std::cout << " " << name;
        if (infinite && name == positions.name_for_position(1000)) {
            std::cout << " [" + name + "] ...";
            break;
        }
    }
    std::cout << "\n" << flags << "\n";
}

CmdLineProgram::CmdLineProgram(std::string name)
    : name_(name), flags_("Options"), positional_("Arguments")
{}

CmdLineProgram& CmdLineProgram::flag(std::string name, std::string description)
{
    flags_.add_options()(name.c_str(), description.c_str());
    return *this;
}

template<class T>
CmdLineProgram&
CmdLineProgram::option(std::string name, std::string description)
{
    flags_.add_options()(name.c_str(), po::value<T>(), description.c_str());
    return *this;
}

template<class T>
CmdLineProgram& CmdLineProgram::option(
    std::string name, std::string description, T default_value)
{
    flags_.add_options()(name.c_str(),
        po::value<T>()->default_value(default_value),
        description.c_str());
    return *this;
}

template<class T>
CmdLineProgram&
CmdLineProgram::arg(std::string name, std::string description, int num)
{
    positional_.add_options()(
        name.c_str(), po::value<T>(), description.c_str());
    positions_.add(name.c_str(), num);
    return *this;
}

bool CmdLineProgram::parse(int argc, char** argv)
{
    po::options_description all("All");
    all.add(flags_).add(positional_);
    po::variables_map vm_;
    po::store(po::command_line_parser(argc, argv)
                  .options(all)
                  .positional(positions_)
                  .run(),
        vm_);
    po::notify(vm_);

    if (vm_.count("help")) {
        print_help(name_, flags_, positional_, positions_);
        return false;
    }

    // TODO: make sure positional args are defined.

    return true;
}

bool CmdLineProgram::defined(std::string opt)
{
    return vm_.count(opt);
}

template<class T>
std::optional<T> CmdLineProgram::get(std::string opt)
{
    if (vm_.count(opt)) {
        std::make_optional(vm_[opt]);
    }
    return std::nullopt;
}

}  // namespace irk
