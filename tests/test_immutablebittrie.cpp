#include <vector>
#include <memory>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#define private public
#define protected public
#include "irkit/immutablebittrie.hpp"

namespace {

class ImmutableBitTrie : public ::testing::Test {
protected:
    //irk::ImmutableBitTrie make_mbt() {
    //    //using NodePtr = irk::MutableBitTrie::NodePtr;
    //    //auto make = [](NodePtr l, NodePtr r, bool sent = false) {
    //    //    return std::make_shared<irk::MutableBitTrie::Node>(l, r, sent);
    //    //};
    //    //NodePtr root = make(
    //    //        make(nullptr, nullptr, true),         // 0
    //    //        make(                                 // 1-
    //    //            nullptr,
    //    //            make(                             // 11-
    //    //                make(nullptr, nullptr, true), // 110
    //    //                make(nullptr, nullptr, true)  // 111
    //    //            )
    //    //        )
    //    //    );
    //    //return irk::MutableBitTrie(root);
    //}
};

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}


