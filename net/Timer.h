#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <queue>
#include "../base/MutexLock.h"
#include "../base/noncopyable.h"

typedef std::function<void()> timeoutCallBack;
class Timer : noncopyable {
public:
    Timer(int timeout, timeoutCallBack callBack);
    ~Timer() {}
    
    void setDeleted() { deleted_ = true; }
    bool isDeleted() { return deleted_; }
    void runCallBack() { if(callBack_) callBack_(); }
    long getTime() { return timeout_; }

private:
    long timeout_;
    timeoutCallBack callBack_;
    bool deleted_;
};

class EventLoop;

// 线程不安全
class TimerHeap : noncopyable {
public:
    typedef std::shared_ptr<Timer> TimerPtr;
    TimerHeap(EventLoop* loop);
    ~TimerHeap();

    void addTimer(int connfd, long timeout, timeoutCallBack callBack);
    void clearTimer(int connfd);
    void handleTimeout();
    struct TimerCmp {
        bool operator()(TimerPtr& ta, TimerPtr& tb) { return ta->getTime() > tb->getTime(); }  
    };
private:
    int timerfd_;
    EventLoop* loop_;
    struct itimerspec howlong_;
    std::unordered_map<int, std::weak_ptr<Timer>> timerMap_;
    std::priority_queue<TimerPtr, std::deque<TimerPtr>, TimerCmp> timerHeap_;
    MutexLock mutex_;

    void setTime(long time);
};

