#pragma once

#include <sys/epoll.h>
#include <functional>
#include <memory>
#include "../Channel.h"
#include "../SocketUtils.h"
#include <iostream>
#include <unistd.h>
void readCallback(int fd, std::shared_ptr<Channel> channel) {

    char buf[65535];
    int len = readn(fd, buf, sizeof(buf));
    if (len != 0)
        writen(fd, buf, len);
    else 
        close(fd);
    channel->setEvent(EPOLLIN | EPOLLET);
}

