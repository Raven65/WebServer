#include <iostream>
#include <string>
#include "WebServer.h"
#include "EventLoop.h"

int main(int argc, char* argv[])
{
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <Port> <Thread Numbers>" << std::endl;
        return -1;
    }
    int port = atoi(argv[1]);
    int threadNum = atoi(argv[2]);
    
    EventLoop loop;
    WebServer webServer(&loop, threadNum, port);
    webServer.start();
    loop.loop();

    return 0;
}

