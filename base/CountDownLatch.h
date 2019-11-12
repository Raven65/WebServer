#pragma once

#include "Condition.h"
#include "MutexLock.h"
#include "noncopyable.h"

class CountDownLatch : noncopyable {
public:
    explicit CountDownLatch(int count) : mutex_(), condition_(mutex_), count_(count) {}
    
    void wait() {
        MutexLockGuard lock(mutex_);
        while (count_ > 0) condition_.wait();
    }

    void countDown() {
        MutexLockGuard lock(mutex_);
        --count_;
        if(count_ == 0) condition_.notifyAll();
    }

private:
    mutable MutexLock mutex_;
    Condition condition_;
    int count_;
};

