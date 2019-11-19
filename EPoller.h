#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include "EventLoop.h"
#include "Channel.h"
#include "base/noncopyable.h"

struct epoll_event;

class EPoller : noncopyable {
public:
    typedef std::vector<struct epoll_event> EventsList;
    typedef std::unordered_map<int, ChannelPtr> ChannelMap;
    typedef std::vector<ChannelPtr> ChannelList;
    EPoller();
    ~EPoller();

    int epoll(int timeout, ChannelList* activeChannels);
    void addChannel(ChannelPtr channel);
    void updateChannel(ChannelPtr channel);
    void removeChannel(ChannelPtr channel);
    
private:
    int epollfd_;
    EventsList events_;
    ChannelMap channels_;
    void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;
};

