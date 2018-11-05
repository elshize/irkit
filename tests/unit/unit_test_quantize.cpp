// MIT License
//
// Copyright (c) 2018 Michal Siedlaczek
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

//! \file
//! \author     Michal Siedlaczek
//! \copyright  MIT License

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <irkit/quantize.hpp>

namespace {

TEST(LinearQuantizer, nonnegative)
{
    irk::LinearQuantizer quantize(
        irk::RealRange(0.0, 100.0), irk::IntegralRange(0, 10));
    ASSERT_THAT(quantize(0.0), 0);
    ASSERT_THAT(quantize(10.0), 1);
    ASSERT_THAT(quantize(70.0), 7);
    ASSERT_THAT(quantize(100.0), 10);
}

TEST(LinearQuantizer, negative)
{
    irk::LinearQuantizer quantize(
        irk::RealRange(-10.0, 90.0), irk::IntegralRange(0, 10));
    ASSERT_THAT(quantize(-10.0), 0);
    ASSERT_THAT(quantize(0.0), 1);
    ASSERT_THAT(quantize(60.0), 7);
    ASSERT_THAT(quantize(90.0), 10);
}

TEST(LinearQuantizer, both_shifted)
{
    irk::LinearQuantizer quantize(
        irk::RealRange(-10.0, 90.0), irk::IntegralRange(1, 11));
    ASSERT_THAT(quantize(-10.0), 1);
    ASSERT_THAT(quantize(0.0), 2);
    ASSERT_THAT(quantize(60.0), 8);
    ASSERT_THAT(quantize(90.0), 11);
}

TEST(LinearQuantizer, outside_of_real_range)
{
    irk::LinearQuantizer quantize(
        irk::RealRange(-10.0, 90.0), irk::IntegralRange(2, 12));
    ASSERT_THAT(quantize(-10.0), 2);
    ASSERT_THAT(quantize(0.0), 3);
    ASSERT_THAT(quantize(60.0), 9);
    ASSERT_THAT(quantize(90.0), 12);
    ASSERT_THAT(quantize(100.0), 13);
    ASSERT_THAT(quantize(-20), 1);
}

}  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
