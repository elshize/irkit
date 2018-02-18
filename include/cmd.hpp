#include <algorithm>
#include <boost/any.hpp>
#include <boost/program_options/option.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "irkit/types.hpp"

namespace irk {

    namespace cmd {

        class NameError : public std::exception {
        protected:
            std::string name_;
            std::string message_;

        public:
            NameError(std::string name, std::string prefix)
                : name_(name), message_(prefix + name_)
            {}
            const char* what() const throw() { return message_.c_str(); }
        };

        class DuplicatedName : public NameError {
        public:
            DuplicatedName(std::string name)
                : NameError(name, "duplicated cmd arguemnt name: ")
            {}
            const char* what() const throw() { return message_.c_str(); }
        };

        class UnrecognizedOption : public NameError {
        public:
            UnrecognizedOption(std::string name)
                : NameError(name, "unrecognized option/flag: ")
            {}
        };

        class MissingValue : public NameError {
        public:
            MissingValue(std::string name)
                : NameError(name, "missing option value: ")
            {}
        };

        class UndefinedRequired : public NameError {
        public:
            UndefinedRequired(std::string name)
                : NameError(name, "undefined required: ")
            {}
        };

        enum OptionType {
            String, Integer
        };

        template<class T>
        struct ByNameHash {
            std::size_t operator()(const T& v) const
            {
                return std::hash<std::string>{}(v.name_);
            }
        };

        struct StringParser {
            std::string operator()(const std::string& val) { return val; }
        };

        class Flag {
        protected:
            std::string name_;
            std::string description_;
            std::optional<char> short_name_;

        public:
            Flag(std::string name, std::string description)
                : name_(name),
                  description_(description),
                  short_name_(std::nullopt)
            {}
            Flag&& add_short(char short_name)
            {
                short_name_ = std::make_optional(short_name);
                return std::move(*this);
            }
            friend class ArgumentParser;
            friend struct ByNameHash<Flag>;
        };

        class Option {
        protected:
            std::string name_;
            std::string description_;
            std::optional<char> short_name_;
            bool required_;
            std::size_t count_;
            std::optional<std::string> default_;

        public:
            Option(std::string name, std::string description)
                : name_(name),
                  description_(description),
                  short_name_(std::nullopt),
                  required_(false),
                  count_(1),
                  default_(std::nullopt)
            {}
            Option&& add_short(char short_name)
            {
                short_name_ = std::make_optional(short_name);
                return std::move(*this);
            }
            Option&& default_value(std::string val)
            {
                default_ = std::make_optional(val);
                return std::move(*this);
            }
            Option&& make_required()
            {
                required_ = true;
                return std::move(*this);
            }
            // TODO:
            //Option&& count(std::size_t n)
            //{
            //    count_ = n;
            //    return std::move(*this);
            //}
            friend class ArgumentParser;
            friend struct ByNameHash<Option>;
        };

        class Argument {
        protected:
            std::string name_;
            std::string description_;
            std::size_t count_;

        public:
            Argument(std::string name, std::string description)
                : name_(name),
                  description_(description),
                  count_(1)
            {}
            Argument& count(std::size_t n)
            {
                count_ = n;
                return *this;
            }
            friend class ArgumentParser;
            friend struct ByNameHash<Argument>;
        };

        class ArgumentMap {
        private:
            std::unordered_map<std::string, std::string> args_;
        public:
            ArgumentMap()
            {}

            bool defined(const std::string& name) const
            {
                return args_.find(name) != args_.end();
            }

            std::string as_string(const std::string& name)
            {
                return args_[name];
            }

            int as_int(const std::string& name)
            {
                return std::stoi(args_[name]);
            }

            friend class ArgumentParser;
        };

        class ArgumentParser {
        private:
            std::string name_;
            std::string description_;
            std::unordered_map<std::string, Flag> flags_;
            std::unordered_map<std::string, Option> options_;
            std::vector<Argument> arguments_;
            std::unordered_map<char, std::string> short_to_long_;
            std::unordered_map<std::string, bool> all_names_;

            void verify_full(const std::string& name)
            {
                if (all_names_.find(name) != all_names_.end()) {
                    throw DuplicatedName(name);
                }
            }

            void verify_short(std::optional<char> name)
            {
                if (!name.has_value()) {
                    return;
                }
                if (short_to_long_.find(name.value()) != short_to_long_.end()) {
                    std::string short_name = "";
                    short_name.push_back(name.value());
                    throw DuplicatedName(short_name);
                }
            }

            void verify_and_update_names(const std::string& name,
                std::optional<char> short_name,
                bool is_flag)
            {
                verify_full(name);
                verify_short(short_name);
                all_names_.emplace(name, is_flag);
                if (short_name.has_value()) {
                    short_to_long_.emplace(short_name.value(), name);
                }
            }

            int parse_positional(
                int argc, char** argv, int argn, ArgumentMap& argmap)
            {
                // TODO
                return argn + 1;
            }

            int parse_flag_or_option(int argc,
                char** argv,
                int argn,
                ArgumentMap& argmap,
                const std::string& name)
            {
                auto pos = all_names_.find(name);
                if (pos == all_names_.end()) {
                    throw UnrecognizedOption(std::string(argv[argn]));
                }
                bool is_flag = pos->second;
                if (is_flag) {
                    argmap.args_[name] = true;
                    return argn + 1;
                } else {
                    if (argn + 1 == argc) {
                        throw MissingValue(std::string(argv[argn]));
                    }
                    argmap.args_[name] = std::string(argv[argn + 1]);
                    return argn + 2;
                }
            }

            int parse_flag_or_option(
                int argc, char** argv, int argn, ArgumentMap& argmap)
            {
                char* arg = argv[argn];
                switch (arg[1]) {
                case '\0':
                    throw UnrecognizedOption(std::string(arg));
                case '-':
                    return parse_flag_or_option(
                        argc, argv, argn, argmap, std::string(arg + 2));
                default:
                    if (arg[2] != '\0') {
                        throw UnrecognizedOption(std::string(arg));
                    }
                    return parse_flag_or_option(
                        argc, argv, argn, argmap, short_to_long_[arg[1]]);
                }
            }

        public:
            ArgumentParser(std::string name, std::string description)
                : name_(name), description_(description)
            {}

            void add_flag(Flag && flag)
            {
                verify_and_update_names(flag.name_, flag.short_name_, true);
                flags_.emplace(flag.name_, flag);
            }

            void add_option(Option && option)
            {
                verify_and_update_names(
                    option.name_, option.short_name_, false);
                options_.emplace(option.name_, option);
            }

            void add_argument(Argument && argument)
            {
                verify_and_update_names(argument.name_, std::nullopt, false);
                arguments_.emplace_back(argument);
            }

            ArgumentMap parse(int argc, char** argv)
            {
                ArgumentMap argmap;
                int argn = 0;
                while (argn < argc) {
                    if (argv[argn][0] == '-') {
                        argn = parse_flag_or_option(argc, argv, argn, argmap);
                    } else {
                        argn = parse_positional(argc, argv, argn, argmap);
                    }
                }
                for (const auto & [name, option] : options_) {
                    if (!argmap.defined(name)) {
                        if (option.default_.has_value()) {
                            argmap.args_[name] = option.default_.value();
                        } else {
                            throw UndefinedRequired(name);
                        }
                    }
                }
                return argmap;
            };
        };

    };  // namespace cmd

    namespace po = boost::program_options;

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

    class CmdLineProgram {
    private:
        std::string name_;
        po::options_description flags_;
        po::options_description positional_;
        po::positional_options_description positions_;
        po::variables_map vm_;

    public:
        CmdLineProgram(std::string name)
            : name_(name), flags_("Options"), positional_("Arguments")
        {}
        CmdLineProgram& flag(std::string name, std::string description)
        {
            flags_.add_options()(name.c_str(), description.c_str());
            return *this;
        }
        template<class T>
        CmdLineProgram& option(std::string name, std::string description)
        {
            flags_.add_options()(
                name.c_str(), po::value<T>(), description.c_str());
            return *this;
        }
        template<class T>
        CmdLineProgram&
        option(std::string name, std::string description, T default_value)
        {
            flags_.add_options()(name.c_str(),
                po::value<T>()->default_value(default_value),
                description.c_str());
            return *this;
        }
        template<class T>
        CmdLineProgram&
        arg(std::string name, std::string description, int num = 1)
        {
            positional_.add_options()(
                name.c_str(), po::value<T>(), description.c_str());
            positions_.add(name.c_str(), num);
            return *this;
        }
        bool parse(int argc, char** argv)
        {
            po::options_description all("All");
            all.add(flags_).add(positional_);
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
        bool defined(std::string opt) { return vm_.count(opt) > 0; }
        std::size_t count(std::string opt) { return vm_.count(opt); }
        template<class T>
        std::optional<T> get(std::string opt)
        {
            if (vm_.count(opt)) {
                return std::make_optional(vm_[opt].as<T>());
            }
            return std::nullopt;
        }
};

}  // namespace irkit

