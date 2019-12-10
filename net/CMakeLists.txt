 set(NET
     EventLoop.cpp
     EventLoopThread.cpp
     EventLoopThreadPool.cpp
     Channel.cpp
     EPoller.cpp
     Timer.cpp
     SocketUtils.cpp
     )
 add_library(net ${NET})
 target_link_libraries(net base)
