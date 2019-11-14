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
set(CMAKE_CXX_FLAGS_RELEASE "-O0")

string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")

add_subdirectory(base)
add_subdirectory(tests)

set(SRC
    main.cpp
    EventLoop.cpp
    Channel.cpp
    EPoller.cpp)

add_executable(WebServer ${SRC})
target_link_libraries(WebServer base)

set(TEST
    EventLoop.cpp
    Channel.cpp
    EPoller.cpp)

add_library(forTest ${TEST})
target_link_libraries(forTest base)
