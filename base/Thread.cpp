#include "Thread.h"

#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/prctl.h>
#include <stdint.h>
#include <stdio.h>
#include <memory>

pid_t gettid() { return static_cast<pid_t>(syscall(SYS_gettid)); }

namespace CurrentThread {
__thread int t_cachedTid = 0;
__thread char t_tidString[32];
__thread int t_tidStringLength = 6;
__thread const char* t_threadName = "default";

void cacheTid() {
    if(t_cachedTid == 0) {
        t_cachedTid = gettid();
        t_tidStringLength = snprintf(t_tidString, sizeof(t_tidString), "%5d ", t_cachedTid);
    }
}
inline int tid() {
    if(__builtin_expect(t_cachedTid == 0, 0)) {
        cacheTid();
    }
    return t_cachedTid;
}

inline const char* tidString() { return t_tidString; }

inline int tidStringLength() { return t_tidStringLength; }

inline const char* name() { return t_threadName; }

}



struct ThreadData {
    Thread::ThreadFunc func_;
    std::string name_;
    pid_t* tid_;
    
    ThreadData(const Thread::ThreadFunc& func, const std::string& name, pid_t* tid)
        : func_(func), name_(name), tid_(tid) {}

    void runInThread() {
        *tid_ = CurrentThread::tid();
        CurrentThread::t_threadName = name_.c_str();
        prctl(PR_SET_NAME, CurrentThread::t_threadName);
        func_();
        
    }
};

void* startThread(void* obj) {
    ThreadData* data = static_cast<ThreadData*>(obj);
    data->runInThread();
    delete data;
    return NULL;
}

Thread::Thread(const ThreadFunc& func, const std::string& name)
    : started_(false),
    joined_(false),
    pthreadId_(0),
    tid_(0),
    func_(func),
    name_(name) {
    setDefaultName();   
}

Thread::~Thread() {
    if(started_ && !joined_) pthread_detach(pthreadId_);
}

void Thread::setDefaultName() {
    if(name_.empty()){
        char buf[32];
        snprintf(buf, sizeof(buf), "Thread");
        name_ = buf;
    }
}

void Thread::start() {
    assert(!started_);
    started_ = true;
    ThreadData* data = new ThreadData(func_, name_, &tid_);
    if(pthread_create(&pthreadId_, NULL, &startThread, data)) {
        started_ = false;
        delete data;
    }
}

int Thread::join() {
    assert(started_);
    assert(!joined_);
    joined_ = true;
    return pthread_join(pthreadId_, NULL);
}
