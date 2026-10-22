cmake_minimum_required(VERSION 3.16)
project(order_client LANGUAGES CXX)

add_executable(order_client src/main.cpp)
target_compile_options(order_client PRIVATE -std=c++17)
target_link_libraries(order_client PRIVATE ws2_32)

