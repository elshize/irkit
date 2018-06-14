cmake_minimum_required(VERSION 2.6)
project(bovw)

add_executable(server server.cpp)
target_link_libraries(server PUBLIC irkit GSL ranges debug_assert ${Boost_LIBRARIES} mongoose)

add_executable(termhits termhits.cpp)
target_link_libraries(termhits PUBLIC irkit GSL ranges debug_assert ${Boost_LIBRARIES})

add_executable(maxscores maxscores.cpp)
target_link_libraries(maxscores PUBLIC irkit GSL ranges debug_assert ${Boost_LIBRARIES})
