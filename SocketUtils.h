#pragma once
#include <stdlib.h>
int setNonBlocking(int fd);
void setNoLinger(int fd);
void setNodelay(int fd);
void ignoreSigpipe();
ssize_t readn(int fd, void* buff, size_t n);
ssize_t writen(int fd, const void* buff, size_t n);

