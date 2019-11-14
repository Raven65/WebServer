#include "Channel.h"

#include <sys/epoll.h>
#include "EventLoop.h"

Channel::Channel(EventLoop* loop, int fd)
    :loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1) {}

Channel::~Channel() {}

void Channel::handleEvent() {
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
        events_ = 0;
        return;
    }
    if (revents_ & EPOLLERR) {
        if (errorCallback_) errorCallback_();
    }
    if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
        if(readCallback_) readCallback_();
    }
    if(revents_ & EPOLLOUT) {
        if(writeCallback_) writeCallback_();
    }
}

