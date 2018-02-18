#include <vector>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#define private public
#define protected public
#include "cmd.hpp"

namespace {

char** create_argv(std::vector<std::string>& args)
{
    char** argv = new char*[args.size()];
    int i = 0;
    for (auto& arg : args) {
        argv[i++] = arg.data();
    }
    return argv;
}

TEST(ArgumentParser, duplicates)
{
    irk::cmd::ArgumentParser parser("program", "Description");
    parser.add_flag({"flag", "Description"});
    ASSERT_THROW(
        parser.add_argument({"flag", "Description"}), irk::cmd::DuplicatedName);
}

TEST(ArgumentParser, one_flag_short)
{
    irk::cmd::ArgumentParser parser("program", "Description");
    parser.add_flag(irk::cmd::Flag{"flag", "Description"}.add_short('f'));
    std::vector<std::string> args = {"-f"};
    char** argv = create_argv(args);
    auto argmap = parser.parse(1, argv);
    ASSERT_EQ(argmap.defined("flag"), true);
    delete[] argv;
}

TEST(ArgumentParser, one_flag_long)
{
    irk::cmd::ArgumentParser parser("program", "Description");
    parser.add_flag(irk::cmd::Flag{"flag", "Description"}.add_short('f'));
    std::vector<std::string> args = {"--flag"};
    char** argv = create_argv(args);
    auto argmap = parser.parse(1, argv);
    ASSERT_EQ(argmap.defined("flag"), true);
    delete[] argv;
}

TEST(ArgumentParser, one_flag_unrecognized)
{
    irk::cmd::ArgumentParser parser("program", "Description");
    parser.add_flag(irk::cmd::Flag{"flag", "Description"}.add_short('f'));
    std::vector<std::string> args = {"--blag"};
    char** argv = create_argv(args);
    ASSERT_THROW(parser.parse(1, argv), irk::cmd::UnrecognizedOption);;
    delete[] argv;
}

TEST(ArgumentParser, one_option_short)
{
    irk::cmd::ArgumentParser parser("program", "Description");
    parser.add_option(
        irk::cmd::Option{"option", "Description"}.add_short('o'));
    std::vector<std::string> args = {"-o", "option_value"};
    char** argv = create_argv(args);
    auto argmap = parser.parse(2, argv);
    ASSERT_EQ(argmap.defined("option"), true);
    delete[] argv;
}

TEST(ArgumentParser, one_option_long)
{
    irk::cmd::ArgumentParser parser("program", "Description");
    parser.add_option(
        irk::cmd::Option{"option", "Description"}.add_short('o'));
    std::vector<std::string> args = {"--option", "option_value"};
    char** argv = create_argv(args);
    auto argmap = parser.parse(2, argv);
    ASSERT_EQ(argmap.defined("option"), true);
    delete[] argv;
}

TEST(ArgumentParser, one_option_unrecognized)
{
    irk::cmd::ArgumentParser parser("program", "Description");
    parser.add_option(irk::cmd::Option{"option", "Description"}.add_short('o'));
    std::vector<std::string> args = {"--obtion"};
    char** argv = create_argv(args);
    ASSERT_THROW(parser.parse(1, argv), irk::cmd::UnrecognizedOption);
    ;
    delete[] argv;
}

TEST(ArgumentParser, one_option_short_unrecognized)
{
    irk::cmd::ArgumentParser parser("program", "Description");
    parser.add_option(irk::cmd::Option{"option", "Description"});
    std::vector<std::string> args = {"-k"};
    char** argv = create_argv(args);
    ASSERT_THROW(parser.parse(1, argv), irk::cmd::UnrecognizedOption);;
    delete[] argv;
}

TEST(ArgumentParser, default_option)
{
    irk::cmd::ArgumentParser parser("program", "Description");
    parser.add_option(
        irk::cmd::Option{"option", "Description"}.default_value("def"));
    std::vector<std::string> args = {};
    char** argv = create_argv(args);
    auto argmap = parser.parse(1, argv);
    ASSERT_EQ(argmap.defined("option"), true);
    ASSERT_EQ(argmap.as_string("option"), "def");
    delete[] argv;
}

TEST(ArgumentParser, int_option)
{
    irk::cmd::ArgumentParser parser("program", "Description");
    parser.add_option(
        irk::cmd::Option{"option", "Description"}.default_value("5"));
    std::vector<std::string> args = {};
    char** argv = create_argv(args);
    auto argmap = parser.parse(1, argv);
    ASSERT_EQ(argmap.defined("option"), true);
    ASSERT_EQ(argmap.as_int("option"), 5);
    delete[] argv;
}

//TEST(ArgumentParser, default_option)
//{
//    irkit::cmd::ArgumentParser parser("program", "Description");
//    parser.add_option(
//        irkit::cmd::Option{"option", "Description"}.default_value("def"));
//    std::vector<std::string> args = {};
//    char** argv = create_argv(args);
//    auto argmap = parser.parse(1, argv);
//    ASSERT_EQ(argmap.defined("option"), true);
//    delete[] argv;
//}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

