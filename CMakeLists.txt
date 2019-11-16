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
    main.cpp)

add_executable(WebServer ${SRC})
target_link_libraries(WebServer net)

set(NET
    EventLoop.cpp
    EventLoopThread.cpp
    EventLoopThreadPool.cpp
    Channel.cpp
    EPoller.cpp
    SocketUtils.cpp
    Server.cpp)

add_library(net ${NET})
target_link_libraries(net base)
