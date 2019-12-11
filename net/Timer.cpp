#include "Timer.h"

#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <memory.h>
#include <time.h>
#include <unistd.h>
#include "EventLoop.h"
#include "Channel.h"
#include "../base/Logger.h"

Timer::Timer(int timeout, timeoutCallBack callBack)
    : callBack_(std::move(callBack)), deleted_(false) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    timeout_ = ts.tv_sec * 1000 + ts.tv_nsec / 1000 / 1000 + timeout;
}

bool Timer::isTimeout() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    long now = ts.tv_sec * 1000 + ts.tv_nsec / 1000 / 1000;
    return now >= timeout_;
}

TimerHeap::TimerHeap(EventLoop* loop) 
    : timerfd_(timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC)),
      loop_(loop), 
      mutex_() {
    assert(timerfd_ != -1);
    bzero(&howlong_, sizeof howlong_);
    ChannelPtr channel(new Channel(loop_, timerfd_));
    channel->setEvent(EPOLLIN | EPOLLET);
    channel->setReadCallback(std::bind(&TimerHeap::handleTimeout, this));
    loop_->runInLoop(std::bind(&EventLoop::addChannel, loop_, channel));
}

TimerHeap::~TimerHeap() { close(timerfd_); }

void TimerHeap::addTimer(int connfd, long timeout, timeoutCallBack callBack) {
    if (connfd >= 0 && timerMap_.find(connfd) != timerMap_.end()) {
        TimerPtr tmp(timerMap_[connfd].lock());
        if (tmp) tmp->setDeleted();
    }
    TimerPtr timer(new Timer(timeout, std::move(callBack)));
    if (connfd >= 0) timerMap_[connfd] = timer;
    timerHeap_.push(timer);
    if (timerHeap_.size() == 1) { 
        setTime(timer->getTime());
    }
}

void TimerHeap::clearTimer(int connfd) {
    if (connfd >= 0 && timerMap_.find(connfd) != timerMap_.end()) {
        TimerPtr timer(timerMap_[connfd].lock());
        if (timer) {
            timer->setDeleted();
        }
    }
}

void TimerHeap::handleTimeout() {
    uint64_t one;
    ssize_t size = read(timerfd_, &one, sizeof one);
    if (size != sizeof(uint64_t)) {
        LOG << "Timeout Error";
    }
    while (!timerHeap_.empty()) {
        TimerPtr tnow = timerHeap_.top();
        if (tnow->isDeleted()) {
            timerHeap_.pop();
        } else if (tnow->isTimeout()) {
            timerHeap_.pop();
            LOG << "Connection Timeout!";
            loop_->runInLoop(std::bind(&Timer::runCallBack, tnow));
        } else {
            setTime(tnow->getTime());
        }
    }
}

void TimerHeap::setTime(long time) {
    howlong_.it_value.tv_sec = time / 1000;
    howlong_.it_value.tv_nsec = (time % 1000) * 1000 * 1000;
    timerfd_settime(timerfd_, TFD_TIMER_ABSTIME, &howlong_, NULL);  
}
