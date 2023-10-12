cmake_minimum_required(VERSION 3.26)
project(ComputerNetwork)

set(CMAKE_CXX_STANDARD 17)

add_executable(ComputerNetwork main.cpp
        server.cpp
        server.h
        client.cpp
        client.h)

target_link_libraries(ComputerNetwork ws2_32)