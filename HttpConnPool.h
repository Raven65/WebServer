#pragma once

#include <list>
#include <memory>
#include <unordered_map>
#include <unistd.h>
#include "net/EventLoop.h"
#include "HttpConn.h"
class HttpConnPool
{
public:
    HttpConnPool(EventLoop* loop, WebServer* server, int connNum) : baseLoop_(loop), server_(server), connNum_(connNum), started_(false) {}
    ~HttpConnPool() {}

    void start() {
       baseLoop_->assertInLoopThread();
       started_ = true;
       for (int i = 0; i < connNum_; ++i) {
           HttpConnPtr hc(new HttpConn(server_, -1));
           freeConnPool_.push_back(std::move(hc));
       }
    }
    HttpConnPtr getNextConn() {
        baseLoop_->assertInLoopThread();
        assert(started_);
        if (!freeConnPool_.empty()) {
            HttpConnPtr tmp = freeConnPool_.front();
            usedConnMap_[tmp->id()] = tmp;
            freeConnPool_.pop_front();
            return tmp;
        } else { 
            HttpConnPtr tmp(new HttpConn(server_, -1));
            usedConnMap_[tmp->id()] = tmp;
            return tmp;
        }
    }
    void addFreeConn(HttpConnPtr conn) {
        baseLoop_->assertInLoopThread();
        freeConnPool_.push_back(conn);
        usedConnMap_.erase(conn->id());
    }
private:
    EventLoop* baseLoop_;
    WebServer* server_;
    int connNum_;
    bool started_;
    std::list<HttpConnPtr> freeConnPool_;
    std::unordered_map<int, HttpConnPtr> usedConnMap_;
    int httpConnId;
};

