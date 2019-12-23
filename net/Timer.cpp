#include "Timer.h"

#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <memory.h>
#include <time.h>
#include <unistd.h>
#include "EventLoop.h"
#include "Channel.h"
#include "../base/Logger.h"

Timer::Timer(long timeout, timeoutCallBack callBack)
    : timeout_(timeout), callBack_(std::move(callBack)), deleted_(false) {
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
    if (timeFlag_) {
        struct timespec tv;
        clock_gettime(CLOCK_MONOTONIC, &tv);
        timeCache_ = tv.tv_sec * 1000 + tv.tv_nsec / 1000 / 1000;
        timeFlag_ = false;
    }
    TimerPtr timer(new Timer(timeCache_ + timeout, std::move(callBack)));
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
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    long now = tv.tv_sec * 1000 + tv.tv_nsec / 1000 / 1000;
    while (!timerHeap_.empty()) {
        TimerPtr tnow = timerHeap_.top();
        if (tnow->isDeleted()) {
            timerHeap_.pop();
//            LOG <<"delete";
        } else if (now >= tnow->getTime()) {
            timerHeap_.pop();
            LOG << "Connection Timeout!";
            tnow->runCallBack();
        } else {
            setTime(tnow->getTime());
        }
    }
}

void TimerHeap::setTime(long time) {
    howlong_.it_value.tv_sec = time / 1000;
    howlong_.it_value.tv_nsec = (time % 1000) * 1000 * 1000;
    timerfd_settime(timerfd_, TFD_TIMER_ABSTIME , &howlong_, NULL);  
}
