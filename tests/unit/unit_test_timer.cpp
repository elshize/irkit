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

#include <chrono>
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <irkit/timer.hpp>

namespace {

TEST(run_with_timer, no_return)
{
    auto slept_for = irk::run_with_timer<std::chrono::milliseconds>(
        []() { std::this_thread::sleep_for(std::chrono::milliseconds(10)); });
    ASSERT_EQ(static_cast<int>(slept_for.count()), 10);
}

TEST(run_with_timer, handler)
{
    int elapsed;
    int result = irk::run_with_timer_ret<std::chrono::milliseconds>(
        []() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return 70;
        },
        [&elapsed](auto const& time) {
            elapsed = static_cast<int>(time.count());
        });
    ASSERT_EQ(result, 70);
}

TEST(run_with_timer, returning)
{
    auto [result, time] =
        irk::run_with_timer_ret<std::chrono::milliseconds>([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return 70;
        });
    (void)time;
    ASSERT_EQ(result, 70);
}

TEST(format, time)
{
    int64_t hours = 1;
    int64_t mins = 15;
    int64_t secs = 57;
    int64_t mills = 124;
    int64_t t = mills + secs * 1000 + mins * 1000 * 60 + hours * 1000 * 60 * 60;
    std::chrono::milliseconds time{t};
    ASSERT_THAT(irk::format_time(time), ::testing::StrEq("01:15:57.124"));
}

}  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
