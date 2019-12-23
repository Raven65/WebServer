cmake_minimum_required(VERSION 2.8)

project(WebServer)

set(CXX_FLAGS
    -g
    -Wall
    -std=c++11
    -D_PTHREADS
    -Wno-unused-parameter
)

set(CMAKE_CXX_COMPLIER "g++")
set(CMAKE_CXX_FLAGS_DEGUG "-O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")

add_subdirectory(base)
add_subdirectory(tests)
add_subdirectory(net)

set(WEB
    WebServer.cpp
    HttpConn.cpp)

add_library(web ${WEB})
target_link_libraries(web net)


add_executable(WebServer main.cpp)
target_link_libraries(WebServer web)

