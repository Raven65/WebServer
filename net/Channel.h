#pragma once

#include <functional>
#include <memory>

class EventLoop;

class Channel : public std::enable_shared_from_this<Channel> {
public:
    typedef std::function<void()> EventCallback;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    void handleEvent();
    void setReadCallback(EventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }

    int fd() const { return fd_; }
    int events() const { return events_; }
    void setRevents(int revt) { revents_ = revt; }
    bool isNoneEvent() const { return events_ == 0; }

    void setEvent(int ev) { events_ = ev; }
    void update();
    void clearAll();

    int index() { return index_; }
    void setIndex(int idx) { index_ = idx; }

    EventLoop* ownerLoop() { return loop_; }
    void setLoop(EventLoop* loop) { loop_ = loop; }    

private:

    EventLoop* loop_;
    const int fd_;
    int events_;
    int revents_;
    int index_;
    
    EventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback errorCallback_;
    EventCallback closeCallback_;
};

typedef std::shared_ptr<Channel> ChannelPtr;
