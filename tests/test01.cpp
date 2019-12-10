#include <iostream>
#include <sys/timerfd.h>
#include "../net/EventLoop.h"
#include "../net/Channel.h"
#include <sys/epoll.h>
#include <memory.h>
#include <memory>
#include <unistd.h>
EventLoop* g_loop;
void timeout() {
    std::cout << "Timeout!" << std::endl;
    g_loop->quit();
}
int main()
{
    EventLoop loop;
    g_loop = &loop;
    int timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    std::shared_ptr<Channel> channel = std::shared_ptr<Channel>(new Channel(&loop, timerfd));
    channel->setReadCallback(timeout);
    channel->setEvent(EPOLLIN);
    loop.addChannel(channel);

    struct itimerspec howlong;
    bzero(&howlong, sizeof howlong);
    howlong.it_value.tv_sec = 5;
    timerfd_settime(timerfd, 0, &howlong, NULL);

    loop.loop();
    close(timerfd);
    return 0;
}

