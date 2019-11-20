#pragma once

#include <assert.h>
#include <functional>
#include <vector>
#include <memory>
#include "EPoller.h"
#include "base/Thread.h"
#include "base/noncopyable.h"

class Channel;
class EPoller;
class TimerHeap;

class EventLoop : noncopyable {
public:
    typedef std::function<void()> Functor;
    typedef std::shared_ptr<Channel> ChannelPtr;
    typedef std::vector<ChannelPtr> ChannelList;
    EventLoop();
    ~EventLoop();
    void loop();
    void quit();
    void runInLoop(Functor cb);
    void queueInLoop(Functor cb);
    void assertInLoopThread() { assert(isInLoopThread()); }
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
    
    void addChannel(ChannelPtr channel);
    void updateChannel(ChannelPtr channel);
    void removeChannel(ChannelPtr channel);

    void addTimer(int connfd, long timeout, Functor f);
    void clearTimer(int connfd);
    
    static EventLoop* getEventLoopofCurrentThread();
private:
    bool looping_;
    bool quit_;
    bool callingPendingFunctors_;
    std::shared_ptr<EPoller> poller_;
    std::shared_ptr<TimerHeap> timer_;
    ChannelList activeChannels_;
    const pid_t threadId_;
    
    int wakeupFd_;
    ChannelPtr wakeupChannel_;
    mutable MutexLock mutex_;
    std::vector<Functor> pendingFuntors_;
    void wakeup();
    void handleRead();
    void doPendingFuntors();

};

