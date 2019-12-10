#include "SocketUtils.h"
#include <fcntl.h>
#include <csignal>
#include <cstring>
#include <cassert>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>

int setNonBlocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    if(fcntl(fd, F_SETFL, new_option) == -1) return -1;
    return old_option;
}

void ignoreSigpipe() {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    assert(sigaction(SIGPIPE, &sa, NULL) != -1);
}

ssize_t readn(int fd, void* buff, size_t n) {
    size_t nleft = n;
    ssize_t nread = 0;
    char* buf = (char*)buff;
    
    while (nleft > 0) {
        if ((nread = read(fd, buf, nleft)) < 0) {
            if (errno == EINTR) {
                nread = 0;
            } else if (errno == EAGAIN) {
                break;
            } else {
                return -1;   
            }
        } else if (nread == 0) {
            break;
        }
        
        nleft -= nread;
        buf += nread;
    }
    return n - nleft;
}

ssize_t writen(int fd, const void* buff, size_t n) {
    size_t nleft = n;
    ssize_t nwritten;
    const char* buf = (const char*)buff;
    
    while (nleft > 0) {
        if ((nwritten = write(fd, buf, nleft)) <= 0) {
            if (nwritten < 0 && errno == EINTR) {
                nwritten = 0;
            } else if (errno == EAGAIN) {
                break;
            } else {
                return -1;
            }
        } 
        nleft -= nwritten;
        buf += nwritten;
    }
    return n - nleft;
}

void setNodelay(int fd) {
    int enable = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void*)&enable, sizeof(enable));
}

void setNoLinger(int fd) {
    struct linger linger_;
    linger_.l_onoff = 1;
    linger_.l_linger = 30;
    setsockopt(fd, SOL_SOCKET, SO_LINGER, (const char *)&linger_, sizeof(linger_));
}
