#pragma once
#include <memory>
#include "Channel.h"
#include "EventLoop.h"
#include "EventLoopThreadPool.h"

class Server
{
public:
    Server(EventLoop *loop, int threadNum_, int port);
    ~Server() {}
    EventLoop *mainLoop() const { return loop_; }
    void start();
    void connectionCallback();

private:
    EventLoop* loop_;
    int threadNum_;
    std::unique_ptr<EventLoopThreadPool> reactorPool_;
    bool started_;
    std::shared_ptr<Channel> acceptChannel_;
    int port_;
    int listenFd_;
    static const int MAXFD = 100000;
};

