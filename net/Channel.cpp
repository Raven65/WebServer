#include "Channel.h"

#include <sys/epoll.h>
#include "../base/Logger.h"
#include "EventLoop.h"
#include <iostream>
Channel::Channel(EventLoop* loop, int fd)
    :loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1) {}

Channel::~Channel() {}

void Channel::handleEvent() {
    if (fd_ == -1) return;
    ChannelPtr guard(shared_from_this());
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
        if(closeCallback_) closeCallback_();
    }
    if (revents_ & EPOLLERR) {
        if (errorCallback_) errorCallback_();
        else if (closeCallback_) closeCallback_();
    }
    if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
        if(readCallback_) readCallback_();
    }
    if(revents_ & EPOLLOUT) {
        if(writeCallback_) writeCallback_();
    }
}

void Channel::update() {
    loop_->updateChannel(shared_from_this());
}

void Channel::clearAll() {
    events_ = 0;
    revents_ = 0;
    fd_ = -1;
}

