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

#define CATCH_CONFIG_MAIN

#include <functional>

#include <catch2/catch.hpp>

#include <irkit/iterator/block_iterator.hpp>
#include <irkit/list/standard_block_list.hpp>
#include <irkit/list/vector_block_list.hpp>

using ir::Block_Iterator;
using ir::Blocked_Position;
using ir::Standard_Block_List;
using ir::Vector_Block_List;

TEST_CASE("Blocked_Position", "[blocked][iterator]")
{
    SECTION("Equality operator")
    {
        REQUIRE(Blocked_Position{0, 3} == Blocked_Position{0, 3});
        REQUIRE_FALSE(Blocked_Position{1, 3} == Blocked_Position{0, 3});
        REQUIRE_FALSE(Blocked_Position{0, 2} == Blocked_Position{0, 3});
    }

    SECTION("Inequality operator")
    {
        REQUIRE_FALSE(Blocked_Position{0, 3} != Blocked_Position{0, 3});
        REQUIRE(Blocked_Position{1, 3} != Blocked_Position{0, 3});
        REQUIRE(Blocked_Position{0, 2} != Blocked_Position{0, 3});
    }
}

TEST_CASE("Block_Iterator", "[blocked][iterator]")
{
    auto vec            = std::vector<int>{1, 5, 6, 8, 12, 14, 20, 23};
    using list_type     = Vector_Block_List<int>;
    using iterator_type = Block_Iterator<list_type>;
    list_type list{0, vec, 3};

    SECTION("Prefix increment")
    {
        iterator_type iter = list.begin();
        ++iter;
        REQUIRE(iter == iterator_type{{0, 1}, list});
        ++iter;
        REQUIRE(iter == iterator_type{{0, 2}, list});
        ++iter;
        REQUIRE(iter == iterator_type{{1, 0}, list});
        ++iter;
        REQUIRE(iter == iterator_type{{1, 1}, list});
        ++iter;
        REQUIRE(iter == iterator_type{{1, 2}, list});
        ++iter;
        REQUIRE(iter == iterator_type{{2, 0}, list});
        ++iter;
        REQUIRE(iter == iterator_type{{2, 1}, list});
        ++iter;
        REQUIRE(iter == iterator_type{{2, 2}, list});
        ++iter;
        REQUIRE(iter == iterator_type{{3, 0}, list});
    }

    SECTION("Suffix increment")
    {
        iterator_type iter = list.begin();
        void(iter++);
        REQUIRE(iter == iterator_type{{0, 1}, list});
        void(iter++);
        REQUIRE(iter == iterator_type{{0, 2}, list});
        void(iter++);
        REQUIRE(iter == iterator_type{{1, 0}, list});
        void(iter++);
        REQUIRE(iter == iterator_type{{1, 1}, list});
        void(iter++);
        REQUIRE(iter == iterator_type{{1, 2}, list});
        void(iter++);
        REQUIRE(iter == iterator_type{{2, 0}, list});
        void(iter++);
        REQUIRE(iter == iterator_type{{2, 1}, list});
        void(iter++);
        REQUIRE(iter == iterator_type{{2, 2}, list});
        void(iter++);
        REQUIRE(iter == iterator_type{{3, 0}, list});
    }

    SECTION("Dereference")
    {
        REQUIRE(*iterator_type{{0, 0}, list} == 1);
        REQUIRE(*iterator_type{{0, 1}, list} == 5);
        REQUIRE(*iterator_type{{0, 2}, list} == 6);
        REQUIRE(*iterator_type{{1, 0}, list} == 8);
        REQUIRE(*iterator_type{{1, 1}, list} == 12);
        REQUIRE(*iterator_type{{1, 2}, list} == 14);
        REQUIRE(*iterator_type{{2, 0}, list} == 20);
        REQUIRE(*iterator_type{{2, 1}, list} == 23);
    }

    SECTION("Construct vector")
    {
        auto constructed = std::vector<int>(list.begin(), list.end());
        REQUIRE(constructed.size() == vec.size());
        REQUIRE_THAT(constructed, Catch::Contains(vec));
    }

    SECTION("Advance to 1")
    {
        auto i = GENERATE(range(0, 1));
        SECTION("looking for 0")
        {
            REQUIRE(*std::next(list.begin(), i).advance_to(0) == 1);
        }
        SECTION("looking for 1")
        {
            REQUIRE(*std::next(list.begin(), i).advance_to(1) == 1);
        }
    }

    SECTION("Advance to 5")
    {
        auto i = GENERATE(range(0, 2));
        SECTION("looking for 2")
        {
            REQUIRE(*std::next(list.begin(), i).advance_to(2) == 5);
        }
        SECTION("looking for 5")
        {
            REQUIRE(*std::next(list.begin(), i).advance_to(5) == 5);
        }
    }

    SECTION("Advance to 12")
    {
        auto i = GENERATE( range(0, 5) );
        SECTION("looking for 9")
        {
            REQUIRE(*std::next(list.begin(), i).advance_to(9) == 12);
        }
        SECTION("looking for 12")
        {
            REQUIRE(*std::next(list.begin(), i).advance_to(12) == 12);
        }
    }

    SECTION("Advance to 14")
    {
        auto i = GENERATE( range(0, 6) );
        SECTION("looking for 13")
        {
            REQUIRE(*std::next(list.begin(), i).advance_to(13) == 14);
        }
        SECTION("looking for 14")
        {
            REQUIRE(*std::next(list.begin(), i).advance_to(14) == 14);
        }
    }

    SECTION("Advance to 20")
    {
        auto i = GENERATE( range(0, 6) );
        SECTION("looking for 15")
        {
            REQUIRE(*std::next(list.begin(), i).advance_to(15) == 20);
        }
        SECTION("looking for 19")
        {
            REQUIRE(*std::next(list.begin(), i).advance_to(19) == 20);
        }
        SECTION("looking for 20")
        {
            REQUIRE(*std::next(list.begin(), i).advance_to(20) == 20);
        }
    }

    SECTION("Advance to 23")
    {
        auto i = GENERATE( range(0, 7) );
        SECTION("looking for 21")
        {
            REQUIRE(*std::next(list.begin(), i).advance_to(21) == 23);
        }
        SECTION("looking for 22")
        {
            REQUIRE(*std::next(list.begin(), i).advance_to(22) == 23);
        }
        SECTION("looking for 23")
        {
            REQUIRE(*std::next(list.begin(), i).advance_to(23) == 23);
        }
    }

    SECTION("Advance to end looking for 30")
    {
        auto i = GENERATE( range(0, 8) );
        CAPTURE(i);
        REQUIRE(std::next(list.begin(), i).advance_to(30) == std::end(list));
    }

    SECTION("Find 1")
    {
        auto i = GENERATE( range(0, 1) );
        SECTION("looking for 0")
        {
            auto initial = std::next(list.begin(), i);
            auto pos = initial.next_ge(0);
            REQUIRE(initial == std::next(list.begin(), i));
            REQUIRE(*pos == 1);
        }
        SECTION("looking for 1")
        {
            auto initial = std::next(list.begin(), i);
            auto pos = initial.next_ge(1);
            REQUIRE(initial == std::next(list.begin(), i));
            REQUIRE(*pos == 1);
        }
    }

    SECTION("Find 12")
    {
        auto i = GENERATE(range(0, 5));
        SECTION("looking for 9")
        {
            auto initial = std::next(list.begin(), i);
            auto pos = initial.next_ge(9);
            REQUIRE(initial == std::next(list.begin(), i));
            REQUIRE(*pos == 12);
        }
        SECTION("looking for 12")
        {
            auto initial = std::next(list.begin(), i);
            auto pos = initial.next_ge(12);
            REQUIRE(initial == std::next(list.begin(), i));
            REQUIRE(*pos == 12);
        }
    }

    SECTION("Find 23")
    {
        auto i = GENERATE(range(0, 7));
        SECTION("looking for 21")
        {
            auto initial = std::next(list.begin(), i);
            auto pos = initial.next_ge(21);
            REQUIRE(initial == std::next(list.begin(), i));
            REQUIRE(*pos == 23);
        }
        SECTION("looking for 23")
        {
            auto initial = std::next(list.begin(), i);
            auto pos = initial.next_ge(23);
            REQUIRE(initial == std::next(list.begin(), i));
            REQUIRE(*pos == 23);
        }
    }

    SECTION("Find end looking for 30")
    {
        auto i = GENERATE(range(0, 8));
        CAPTURE(i);
        auto initial = std::next(list.begin(), i);
        auto pos = initial.next_ge(30);
        REQUIRE(initial == std::next(list.begin(), i));
        REQUIRE(pos == list.end());
    }

    SECTION("Fetch until end")
    {
        auto i = GENERATE(range(0, 8));
        CAPTURE(i);
        auto fetched = std::next(list.begin(), i).fetch();
        std::vector<int> lvec(fetched.begin(), fetched.end());
        std::vector<int> rvec(std::next(vec.begin(), i), vec.end());
        REQUIRE(lvec == rvec);
    }

    SECTION("Fetch until second-last")
    {
        auto i = GENERATE(range(0, 7));
        CAPTURE(i);
        auto fetched = std::next(list.begin(), i)
                           .fetch(list.begin().next_ge(23));
        std::vector<int> lvec(fetched.begin(), fetched.end());
        std::vector<int> rvec(std::next(vec.begin(), i), std::prev(vec.end()));
        REQUIRE(lvec == rvec);
    }
}

// TODO(michal): Change to a TEMPLATE_TEST_CASE once available
TEST_CASE("Block_Iterator with Standard_Block_List", "[blocked][iterator]")
{
    auto vec            = std::vector<int>{1, 5, 6, 8, 12, 14, 20, 23};
    using list_type     = Standard_Block_List<int, irk::vbyte_codec<int>, true>;
    using iterator_type = Block_Iterator<list_type>;
    ir::Standard_Block_List_Builder<int, irk::vbyte_codec<int>, true> builder{3};
    for (auto v : vec) {
        builder.add(v);
    }
    std::ostringstream os;
    builder.write(os);
    std::string data = os.str();
    list_type list{0, irk::make_memory_view(data.data(), data.size()), 8};

    SECTION("Prefix increment")
    {
        iterator_type iter = list.begin();
        ++iter;
        REQUIRE(iter == iterator_type{{0, 1}, list});
        ++iter;
        REQUIRE(iter == iterator_type{{0, 2}, list});
        ++iter;
        REQUIRE(iter == iterator_type{{1, 0}, list});
        ++iter;
        REQUIRE(iter == iterator_type{{1, 1}, list});
        ++iter;
        REQUIRE(iter == iterator_type{{1, 2}, list});
        ++iter;
        REQUIRE(iter == iterator_type{{2, 0}, list});
        ++iter;
        REQUIRE(iter == iterator_type{{2, 1}, list});
        ++iter;
        REQUIRE(iter == iterator_type{{2, 2}, list});
        ++iter;
        REQUIRE(iter == iterator_type{{3, 0}, list});
    }

    SECTION("Suffix increment")
    {
        iterator_type iter = list.begin();
        void(iter++);
        REQUIRE(iter == iterator_type{{0, 1}, list});
        void(iter++);
        REQUIRE(iter == iterator_type{{0, 2}, list});
        void(iter++);
        REQUIRE(iter == iterator_type{{1, 0}, list});
        void(iter++);
        REQUIRE(iter == iterator_type{{1, 1}, list});
        void(iter++);
        REQUIRE(iter == iterator_type{{1, 2}, list});
        void(iter++);
        REQUIRE(iter == iterator_type{{2, 0}, list});
        void(iter++);
        REQUIRE(iter == iterator_type{{2, 1}, list});
        void(iter++);
        REQUIRE(iter == iterator_type{{2, 2}, list});
        void(iter++);
        REQUIRE(iter == iterator_type{{3, 0}, list});
    }

    SECTION("Dereference")
    {
        REQUIRE(*iterator_type{{0, 0}, list} == 1);
        REQUIRE(*iterator_type{{0, 1}, list} == 5);
        REQUIRE(*iterator_type{{0, 2}, list} == 6);
        REQUIRE(*iterator_type{{1, 0}, list} == 8);
        REQUIRE(*iterator_type{{1, 1}, list} == 12);
        REQUIRE(*iterator_type{{1, 2}, list} == 14);
        REQUIRE(*iterator_type{{2, 0}, list} == 20);
        REQUIRE(*iterator_type{{2, 1}, list} == 23);
    }

    SECTION("Construct vector")
    {
        auto constructed = std::vector<int>(list.begin(), list.end());
        REQUIRE(constructed.size() == vec.size());
        REQUIRE_THAT(constructed, Catch::Contains(vec));
    }

    SECTION("Advance to 1")
    {
        auto i = GENERATE(range(0, 1));
        SECTION("looking for 0")
        {
            REQUIRE(*std::next(list.begin(), i).advance_to(0) == 1);
        }
        SECTION("looking for 1")
        {
            REQUIRE(*std::next(list.begin(), i).advance_to(1) == 1);
        }
    }

    SECTION("Advance to 5")
    {
        auto i = GENERATE(range(0, 2));
        SECTION("looking for 2")
        {
            REQUIRE(*std::next(list.begin(), i).advance_to(2) == 5);
        }
        SECTION("looking for 5")
        {
            REQUIRE(*std::next(list.begin(), i).advance_to(5) == 5);
        }
    }

    SECTION("Advance to 12")
    {
        auto i = GENERATE( range(0, 5) );
        SECTION("looking for 9")
        {
            REQUIRE(*std::next(list.begin(), i).advance_to(9) == 12);
        }
        SECTION("looking for 12")
        {
            REQUIRE(*std::next(list.begin(), i).advance_to(12) == 12);
        }
    }

    SECTION("Advance to 14")
    {
        auto i = GENERATE( range(0, 6) );
        SECTION("looking for 13")
        {
            REQUIRE(*std::next(list.begin(), i).advance_to(13) == 14);
        }
        SECTION("looking for 14")
        {
            REQUIRE(*std::next(list.begin(), i).advance_to(14) == 14);
        }
    }

    SECTION("Advance to 20")
    {
        auto i = GENERATE( range(0, 6) );
        SECTION("looking for 15")
        {
            REQUIRE(*std::next(list.begin(), i).advance_to(15) == 20);
        }
        SECTION("looking for 19")
        {
            REQUIRE(*std::next(list.begin(), i).advance_to(19) == 20);
        }
        SECTION("looking for 20")
        {
            REQUIRE(*std::next(list.begin(), i).advance_to(20) == 20);
        }
    }

    SECTION("Advance to 23")
    {
        auto i = GENERATE( range(0, 7) );
        SECTION("looking for 21")
        {
            REQUIRE(*std::next(list.begin(), i).advance_to(21) == 23);
        }
        SECTION("looking for 22")
        {
            REQUIRE(*std::next(list.begin(), i).advance_to(22) == 23);
        }
        SECTION("looking for 23")
        {
            REQUIRE(*std::next(list.begin(), i).advance_to(23) == 23);
        }
    }

    SECTION("Advance to end looking for 30")
    {
        auto i = GENERATE( range(0, 8) );
        CAPTURE(i);
        REQUIRE(std::next(list.begin(), i).advance_to(30) == std::end(list));
    }

    SECTION("Find 1")
    {
        auto i = GENERATE( range(0, 1) );
        SECTION("looking for 0")
        {
            auto initial = std::next(list.begin(), i);
            auto pos = initial.next_ge(0);
            REQUIRE(initial == std::next(list.begin(), i));
            REQUIRE(*pos == 1);
        }
        SECTION("looking for 1")
        {
            auto initial = std::next(list.begin(), i);
            auto pos = initial.next_ge(1);
            REQUIRE(initial == std::next(list.begin(), i));
            REQUIRE(*pos == 1);
        }
    }

    SECTION("Find 12")
    {
        auto i = GENERATE(range(0, 5));
        SECTION("looking for 9")
        {
            auto initial = std::next(list.begin(), i);
            auto pos = initial.next_ge(9);
            REQUIRE(initial == std::next(list.begin(), i));
            REQUIRE(*pos == 12);
        }
        SECTION("looking for 12")
        {
            auto initial = std::next(list.begin(), i);
            auto pos = initial.next_ge(12);
            REQUIRE(initial == std::next(list.begin(), i));
            REQUIRE(*pos == 12);
        }
    }

    SECTION("Find 23")
    {
        auto i = GENERATE(range(0, 7));
        SECTION("looking for 21")
        {
            auto initial = std::next(list.begin(), i);
            auto pos = initial.next_ge(21);
            REQUIRE(initial == std::next(list.begin(), i));
            REQUIRE(*pos == 23);
        }
        SECTION("looking for 23")
        {
            auto initial = std::next(list.begin(), i);
            auto pos = initial.next_ge(23);
            REQUIRE(initial == std::next(list.begin(), i));
            REQUIRE(*pos == 23);
        }
    }

    SECTION("Find end looking for 30")
    {
        auto i = GENERATE(range(0, 8));
        CAPTURE(i);
        auto initial = std::next(list.begin(), i);
        auto pos = initial.next_ge(30);
        REQUIRE(initial == std::next(list.begin(), i));
        REQUIRE(pos == list.end());
    }

    SECTION("Fetch until end")
    {
        auto i = GENERATE(range(0, 8));
        CAPTURE(i);
        auto fetched = std::next(list.begin(), i).fetch();
        std::vector<int> lvec(fetched.begin(), fetched.end());
        std::vector<int> rvec(std::next(vec.begin(), i), vec.end());
        REQUIRE(lvec == rvec);
    }

    SECTION("Fetch until second-last")
    {
        auto i = GENERATE(range(0, 7));
        CAPTURE(i);
        auto fetched = std::next(list.begin(), i)
                           .fetch(list.begin().next_ge(23));
        std::vector<int> lvec(fetched.begin(), fetched.end());
        std::vector<int> rvec(std::next(vec.begin(), i), std::prev(vec.end()));
        REQUIRE(lvec == rvec);
    }
}
