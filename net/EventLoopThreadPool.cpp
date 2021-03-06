#include "EventLoopThreadPool.h"

const int MAXFD = 65536;

EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, int numThreads)
    : baseLoop_(baseLoop), started_(false), numThreads_(numThreads), next_(0) {
    if (numThreads_ <= 0) {
    LOG << "numThreads_ <= 0";
    abort();
    }
}

void EventLoopThreadPool::start() {
    baseLoop_->assertInLoopThread();
    started_ = true;
    for (int i = 0; i < numThreads_; ++i) {
        std::shared_ptr<EventLoopThread> t(new EventLoopThread());
        loops_.push_back(t->startLoop());
        threads_.push_back(std::move(t));
    }
}

EventLoop* EventLoopThreadPool::getNextLoop() {
    baseLoop_->assertInLoopThread();
    assert(started_);
    EventLoop* loop = baseLoop_;
    if (!loops_.empty()) {
        loop = loops_[next_]; 
        if (loop->getConnCnt() > MAXFD / 2) {
            next_ = (next_ + 1) % numThreads_;
            loop = loops_[next_];
        } 
        next_ = (next_ + 1) % numThreads_;
    }
    return loop;
}
