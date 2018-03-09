#include <boost/filesystem.hpp>
#include <string>
#include <vector>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#define private public
#define protected public
#include "irkit/alphabetical_bst.hpp"
#include "irkit/coding/hutucker.hpp"
#include "irkit/prefixmap.hpp"
#include "irkit/radix_tree.hpp"

namespace {

TEST(radix_tree, insert)
{
    // TODO
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

