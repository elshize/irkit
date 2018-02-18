cmake_minimum_required(VERSION 2.6)
project(blip)

add_executable(server server.cpp)
target_link_libraries(server stdc++fs mongoose)

add_executable(termhits termhits.cpp)
target_link_libraries(termhits stdc++fs)

add_executable(maxscores maxscores.cpp)
target_link_libraries(maxscores stdc++fs)
