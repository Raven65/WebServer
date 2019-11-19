#include <iostream>
#include "../WebServer.h"

int main(int argc, char* argv[])
{
    EventLoop eventloop;
    WebServer server(&eventloop, 4, atoi(argv[1]));
    server.start();
    eventloop.loop();
    return 0;
}

