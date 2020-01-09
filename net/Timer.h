#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include "../base/MutexLock.h"
#include "../base/noncopyable.h"

typedef std::function<void()> timeoutCallBack;
class Timer : noncopyable {
    friend class TimerHeap;
public:
    struct timeType_ {
        long sec;
        long msec;
        bool operator<(const timeType_& t) const { return sec < t.sec || (sec == t.sec && msec < t.msec); }
        bool operator>=(const timeType_& t) const { return !this->operator<(t); }
    };
    Timer(timeType_ timeout, timeoutCallBack callBack, int heapIndex);
    ~Timer() {}
    
    void setDeleted() { heapIndex_ = -1; callBack_ = std::function<void()>(); }
    bool isDeleted() { return heapIndex_ == -1; }
    void runCallBack() { if(callBack_) callBack_(); }
    timeType_ getTimeout() const { return timeout_; }

private:
    timeType_ timeout_;
    timeoutCallBack callBack_;
    
    int heapIndex_;
};

class EventLoop;

// 线程不安全
class TimerHeap : noncopyable {
public:
    typedef std::shared_ptr<Timer> TimerPtr;
    TimerHeap(EventLoop* loop);
    ~TimerHeap();

    void addTimer(int id, long timeout_ms, timeoutCallBack callBack);
    void removeTimer(int id);
    
    void modify(size_t index);
    void pop(size_t index);

    void upHeap(size_t index);
    void downHeap(size_t index);
    void swapHeap(size_t index1, size_t index2);

    void handleTimeout();
    
    void setTimeFlag() { timeFlag_ = true; }
private:
    int timerfd_;
    EventLoop* loop_;
    struct itimerspec howlong_;
    std::unordered_map<int, std::weak_ptr<Timer>> timerMap_;
    std::vector<TimerPtr> timerHeap_;
    

    Timer::timeType_ timeCache_;
    bool timeFlag_;

    bool setTime(const Timer::timeType_& now, const Timer::timeType_& time);
};

