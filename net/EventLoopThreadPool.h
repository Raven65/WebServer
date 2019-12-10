#pragma once

#include <vector>
#include <memory>
#include "EventLoopThread.h"
#include "../base/Logger.h"
#include "../base/noncopyable.h"

class EventLoopThreadPool : noncopyable {
public:
    EventLoopThreadPool(EventLoop* baseLoop, int numThreads);
    ~EventLoopThreadPool() { LOG << "~EventLoopThreadPool()"; }

    void start();
    EventLoop* getNextLoop();

private:
    EventLoop* baseLoop_;
    bool started_;
    int numThreads_;
    int next_;
    std::vector<std::shared_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop*> loops_;
};

