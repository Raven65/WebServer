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

Timer::Timer(timeType_ timeout, timeoutCallBack callBack, int heapIndex)
    : timeout_(timeout), callBack_(std::move(callBack)), heapIndex_(heapIndex) {
}

TimerHeap::TimerHeap(EventLoop* loop) 
    : timerfd_(timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC)),
      loop_(loop) {
    assert(timerfd_ != -1);
    bzero(&howlong_, sizeof howlong_);
    ChannelPtr channel(new Channel(loop_, timerfd_));
    channel->setEvent(EPOLLIN | EPOLLET);
    channel->setReadCallback(std::bind(&TimerHeap::handleTimeout, this));
    loop_->runInLoop(std::bind(&EventLoop::addChannel, loop_, channel));
}

TimerHeap::~TimerHeap() { close(timerfd_); }

void TimerHeap::addTimer(int id, long timeout_ms, timeoutCallBack callBack) {
    if (timeFlag_) {
        struct timespec tv;
        clock_gettime(CLOCK_MONOTONIC, &tv);
        timeCache_.sec = tv.tv_sec;
        timeCache_.msec = tv.tv_nsec / 1000 / 1000;
        timeFlag_ = false;
    }
    Timer::timeType_ timeout(timeCache_);
    timeout.sec += (timeCache_.msec + timeout_ms) / 1000;
    timeout.msec = (timeCache_.msec + timeout_ms) % 1000;
    if (timerMap_.find(id) != timerMap_.end()) {
        TimerPtr tmp(timerMap_[id].lock());
        if (tmp && tmp->heapIndex_ != -1) {
            tmp->timeout_ = std::move(timeout);
            tmp->callBack_ = std::move(callBack);
            modify(tmp->heapIndex_);
            return;
        }
    }
    TimerPtr timer(new Timer(std::move(timeout), std::move(callBack), timerHeap_.size()));
    timerMap_[id] = timer;
    timerHeap_.push_back(timer);
    upHeap(timerHeap_.size() - 1);
    if (timerHeap_.front() == timer) { 
        setTime(timeCache_, timer->getTimeout());
    }
}

void TimerHeap::removeTimer(int id) {
    if (timerMap_.find(id) != timerMap_.end()) {
        TimerPtr timer(timerMap_[id].lock());
        if (timer && timer->heapIndex_ != -1) {
            size_t index = timer->heapIndex_;
            if (!timerHeap_.empty() && index < timerHeap_.size()) {
                pop(index);    
            }
            timer->heapIndex_ = -1;
        }
    }
}

void TimerHeap::modify(size_t index) {
    size_t parent = (index - 1) / 2;
    if (index > 0 && timerHeap_[index]->timeout_ < timerHeap_[parent]->timeout_)
        upHeap(index);
    else 
        downHeap(index);
}
void TimerHeap::pop(size_t index) {
    if (timerHeap_.size() - 1 == index) {
        timerHeap_.pop_back();
    } else {
        swapHeap(index, timerHeap_.size() - 1);
        timerHeap_.pop_back();
        modify(index);
    }
}

void TimerHeap::upHeap(size_t index) {
    size_t parent = (index - 1) / 2;
    if (index > 0 && timerHeap_[index]->timeout_ < timerHeap_[parent]->timeout_) {
        swapHeap(index, parent);
        index = parent;
        parent = (index - 1) / 2;
    }
}

void TimerHeap::downHeap(size_t index) {
    size_t child = index * 2 + 1;
    while (child < timerHeap_.size()) {
        if (child + 1 < timerHeap_.size() && timerHeap_[child + 1]->timeout_ < timerHeap_[child]->timeout_) 
            ++child;
        if (timerHeap_[index]->timeout_ < timerHeap_[child]->timeout_) 
            break;
        swapHeap(index, child);
        index = child;
        child = index * 2 + 1;
    }
}

void TimerHeap::swapHeap(size_t index1, size_t index2) {
    swap(timerHeap_[index1], timerHeap_[index2]);
}

void TimerHeap::handleTimeout() {
    uint64_t one;
    ssize_t size = read(timerfd_, &one, sizeof one);
    if (size != sizeof(uint64_t)) {
        LOG << "Timeout Error";
    }
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    Timer::timeType_ now = {tv.tv_sec, tv.tv_nsec / 1000 / 1000};
    while (!timerHeap_.empty()) {
        TimerPtr tnow = timerHeap_.front();
        if (tnow->isDeleted()) {
            pop(0);
//            LOG <<"delete";
        } else if (now >= tnow->getTimeout()) {
            pop(0);
            LOG << "Connection Timeout!";
            tnow->runCallBack();
        } else {
            if (setTime(now, tnow->getTimeout())) {
                break;
            } else {
                pop(0);
            }
        }
    }
    if (timerHeap_.empty()) setTime(now, {0, 0});
}

bool TimerHeap::setTime(const Timer::timeType_& now, const Timer::timeType_& time) {
    if (time.sec != 0 && time.msec != 0 && time < now) {
        LOG <<"Timer error ";
        return false;
    }
    howlong_.it_value.tv_sec = time.sec;
    howlong_.it_value.tv_nsec = time.msec * 1000 * 1000;
    if (timerfd_settime(timerfd_, TFD_TIMER_ABSTIME, &howlong_, NULL) == -1) return false; 
    return true;
}
