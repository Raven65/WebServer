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
    Timer(long timeout, timeoutCallBack callBack, int heapIndex);
    ~Timer() {}
    
    void setDeleted() { heapIndex_ = -1; callBack_ = std::function<void()>(); }
    bool isDeleted() { return heapIndex_ == -1; }
    void runCallBack() { if(callBack_) callBack_(); }
    long getTimeout() { return timeout_; }

private:
    long timeout_;
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

    void addTimer(int connfd, long timeout, timeoutCallBack callBack);
    void removeTimer(int connfd);
    
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
    

    long timeCache_;
    bool timeFlag_;

    void setTime(long time);
};

