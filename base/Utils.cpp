#include "Utils.h"
#include <fcntl.h>
#include <csignal>
#include <cstring>
#include <cassert>

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
