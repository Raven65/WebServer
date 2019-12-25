#include "EventLoop.h"
#include <sys/eventfd.h>
#include <unistd.h>
#include <sys/epoll.h>
#include "Timer.h"
#include "../base/Logger.h"

__thread EventLoop* t_loopInThisThread = 0;
const int kTimeoutMs = 10000;

int createEventfd() {
    int evtfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0) {
        LOG << "Error: Failed in eventfd";
        abort();
    }

    return evtfd;
}

EventLoop::EventLoop() 
    : looping_(false),
      quit_(false),
      callingPendingFunctors_(false),
      poller_(new EPoller), 
      threadId_(CurrentThread::tid()), 
      wakeupFd_(createEventfd()), 
      wakeupChannel_(ChannelPtr(new Channel(this, wakeupFd_))),
      connCnt_(0) {
    LOG << "EventLoop created in thread " << threadId_;
    if(!t_loopInThisThread) t_loopInThisThread = this;

    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    wakeupChannel_->setEvent(EPOLLIN | EPOLLET);
    poller_->addChannel(wakeupChannel_);
    timer_ = std::shared_ptr<TimerHeap>(new TimerHeap(this));
}

EventLoop::~EventLoop() {
    assert(!looping_);
    t_loopInThisThread = NULL;
    close(wakeupFd_);
}

EventLoop* EventLoop::getEventLoopofCurrentThread() { return t_loopInThisThread; }

void EventLoop::loop() {
    assert(!looping_);
    assertInLoopThread();
    looping_ = true;
    LOG <<"Thread " << threadId_ << " start looping";
    quit_ = false;
    while (!quit_) {
        activeChannels_.clear();
        timer_->setTimeFlag();
        poller_->epoll(kTimeoutMs, &activeChannels_);
        for (auto channel: activeChannels_) {
            channel->handleEvent();
        }
        doPendingFuntors();
    }

    LOG << "EventLoop in thread " << CurrentThread::tid() << " stop looping";
    looping_ = false;
}

void EventLoop::quit() {
    quit_ = true;
}

void EventLoop::addChannel(ChannelPtr channel) {
    assertInLoopThread();
    poller_->addChannel(std::move(channel));
}

void EventLoop::updateChannel(ChannelPtr channel) {
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    poller_->updateChannel(std::move(channel)); 
}

void EventLoop::removeChannel(ChannelPtr channel) {
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    poller_->removeChannel(std::move(channel));
}

void EventLoop::runInLoop(Functor cb) {
    if (isInLoopThread()) {
        cb();
    } else {
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(Functor cb) {
    {
        MutexLockGuard lock(mutex_);
        pendingFuntors_.push_back(std::move(cb));
    }
    if(!isInLoopThread() || callingPendingFunctors_) {
        wakeup();
    }
}

void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if (n != sizeof one) {
        LOG << "Error: EventLoop::wakeup() writes " << n << " bytes instead of 8";
    }
}

void EventLoop::handleRead() {
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one) {
        LOG << "Error: EventLoop::handleRead() reads" << n << " bytes instead of 8";
    }
}

void EventLoop::doPendingFuntors() {
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;
    
    {
        MutexLockGuard lock(mutex_);
        functors.swap(pendingFuntors_);
    }

    for(const Functor& functor : functors) {
        functor();
    }

    callingPendingFunctors_ = false;
}

void EventLoop::addTimer(int connfd, long timeout, Functor cb) {
//    queueInLoop(std::bind(&TimerHeap::addTimer, timer_, connfd, timeout, std::move(cb)));
    timer_->addTimer(connfd, timeout, std::move(cb));
}

void EventLoop::clearTimer(int connfd) {
    timer_->clearTimer(connfd);
}
