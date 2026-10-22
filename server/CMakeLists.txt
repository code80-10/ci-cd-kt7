cmake_minimum_required(VERSION 3.16)
project(order_server LANGUAGES CXX)

add_executable(order_server src/main.cpp)
target_compile_options(order_server PRIVATE -std=c++17)
target_link_libraries(order_server PRIVATE ws2_32)

