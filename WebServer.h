#pragma once
#include <memory>
#include <unordered_map>
#include "net/Channel.h"
#include "net/EventLoop.h"
#include "net/EventLoopThreadPool.h"
#include "HttpConn.h"

class WebServer
{
public:
    WebServer(EventLoop *loop, int threadNum_, int port);
    ~WebServer() {}
    EventLoop *mainLoop() const { return loop_; }
    void start();
    void connectionCallback();
    void removeConn(const HttpConnPtr& conn);

private:
    EventLoop* loop_;
    int threadNum_;
    std::unique_ptr<EventLoopThreadPool> reactorPool_;
    bool started_;
    std::shared_ptr<Channel> acceptChannel_;
    int port_;
    int listenFd_;
    static const int MAXFD = 100000;

    typedef std::unordered_map<int, HttpConnPtr> ConnectionMap;

    ConnectionMap connMap_;
    void removeConnInLoop(const HttpConnPtr& conn);
};

