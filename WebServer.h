#pragma once
#include <memory>
#include <unordered_map>
#include "base/MutexLock.h"
#include "net/Channel.h"
#include "net/EventLoop.h"
#include "net/EventLoopThreadPool.h"
#include "net/SQLConnection.h"
#include "HttpConnPool.h"

class WebServer
{
public:
    WebServer(EventLoop *loop, int threadNum_, int port);
    ~WebServer() {}
    EventLoop *mainLoop() const { return loop_; }
    void start();
    void connectionCallback();
    void returnConn(HttpConnPtr conn);

    bool existUser(const std::string& user) { MutexLockGuard lock(mutex_); return users.find(user) != users.end(); }
    bool verifyUser(const std::string& user, const std::string& pwd) { MutexLockGuard lock(mutex_); return users[user] == pwd;}
    void updateUser(const std::string& user, const std::string& pwd);

    void cachePage(const std::string& filename);
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

    HttpConnPool connPool_;

    MutexLock mutex_;
    std::unordered_map<std::string, std::string> users;
    std::shared_ptr<SQLConnection> sql;

    void returnConnInLoop(HttpConnPtr conn);
};

