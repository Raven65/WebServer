#include "EPoller.h"

#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>
#include "base/Logger.h"
const int kEventInit = 4096;

EPoller::EPoller() : epollfd_(epoll_create1(EPOLL_CLOEXEC)), events_(kEventInit) {
    if (epollfd_ < 0) {
        LOG << "Error: Epoller";
    }
}

EPoller::~EPoller() {
    close(epollfd_);
}

int EPoller::epoll(int timeout, ChannelList* activeChannels) {

    int numEvents = epoll_wait(epollfd_, events_.data(), static_cast<int>(events_.size()), timeout);

    if (numEvents > 0) {
        // LOG << numEvents <<" events happended";

        fillActiveChannels(numEvents, activeChannels);
        
    } else if (numEvents < 0) {
        LOG << "Error: epoll_wait";
    }

    return numEvents;
}

void EPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const {
    for(int i = 0; i < numEvents; ++i) {
        int fd = events_[i].data.fd;
        auto it = channels_.find(fd);
        assert(it != channels_.end());
        auto channel = it->second;
        channel->setRevents(events_[i].events);
        activeChannels->push_back(channel);
    } 
}

void EPoller::addChannel(ChannelPtr channel) {
    int fd = channel->fd();
    
    assert(channels_.find(fd) == channels_.end());
    channels_[fd] = channel;

    struct epoll_event event;
    memset(&event, 0, sizeof event);
    event.events = channel->events();
    event.data.fd = fd;
    
    if (epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &event) < 0) {
        LOG << "epoll_add error";
        channels_.erase(fd);
    }
    LOG << "Epoll ADD fd = " << fd;
}

void EPoller::updateChannel(ChannelPtr channel) {
    int fd = channel->fd();
    // LOG << "Epoll MOD fd = " << fd;
    
    assert(channels_.find(fd) != channels_.end());
    assert(channels_[fd] == channel);

    struct epoll_event event;
    memset(&event, 0, sizeof event);
    event.events = channel->events();
    event.data.fd = fd;
    if (epoll_ctl(epollfd_, EPOLL_CTL_MOD, fd, &event) < 0) {
        LOG << "epoll_mod error";
        channels_.erase(fd);
    }
}

void EPoller::removeChannel(ChannelPtr channel) {
    int fd = channel->fd();
    assert(channels_.find(fd) != channels_.end());
    assert(channels_[fd] == channel);
    assert(channel->isNoneEvent());

    channels_.erase(fd);
    struct epoll_event event;
    memset(&event, 0, sizeof event);
    event.events = channel->events();
    event.data.fd = fd;
    if (epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, &event) < 0) {
        LOG << "Error: epoll_del error";
    } 
    LOG << "Epoll DEL fd = " << fd;
}
