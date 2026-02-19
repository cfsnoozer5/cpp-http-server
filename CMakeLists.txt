cmake_minimum_required(VERSION 3.16)
project(HttpServer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(http_server
    "src/http_server.cpp"
)

target_include_directories(http_server
    PUBLIC include
)