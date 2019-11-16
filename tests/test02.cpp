#include <iostream>
#include "../Server.h"

int main(int argc, char* argv[])
{
    EventLoop eventloop;
    Server server(&eventloop, 4, atoi(argv[1]));
    server.start();
    eventloop.loop();
    return 0;
}

