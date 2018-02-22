cmake_minimum_required(VERSION 2.6)
project(blip)

add_executable(server server.cpp)
target_link_libraries(server ${Boost_LIBRARIES} mongoose)

add_executable(termhits termhits.cpp)
target_link_libraries(termhits ${Boost_LIBRARIES})

add_executable(maxscores maxscores.cpp)
target_link_libraries(maxscores ${Boost_LIBRARIES})
