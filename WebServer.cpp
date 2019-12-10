#include "WebServer.h"
#include <arpa/inet.h>
#include <functional>
#include <unistd.h>
#include <sys/epoll.h>
#include "net/SocketUtils.h"
#include "base/Logger.h"

WebServer::WebServer(EventLoop* loop, int threadNum, int port)
    : loop_(loop),
      threadNum_(threadNum),
      reactorPool_(new EventLoopThreadPool(loop_, threadNum_)),
      started_(false),
      port_(port) {
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd != -1);
    
    //for DEBUG
    int optval = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    struct sockaddr_in servAddr;
    bzero((char*)&servAddr, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons((unsigned short)port);

    if (bind(listenfd, (struct sockaddr *)&servAddr, sizeof(servAddr)) == -1) {
        LOG << "Error: bind listenfd";
        close(listenfd);
        abort();
    }

    if (listen(listenfd, 2048) == -1) {
        LOG << "Error: listen listenfd";
        close(listenfd);
        abort();
    }
    
    assert(setNonBlocking(listenfd) != -1);
    listenFd_ = listenfd;
    acceptChannel_ = std::shared_ptr<Channel>(new Channel(loop_, listenfd));
}

void WebServer::start() {
    reactorPool_->start();
    
    acceptChannel_->setReadCallback(std::bind(&WebServer::connectionCallback, this));
    acceptChannel_->setEvent(EPOLLIN | EPOLLET);
    loop_->addChannel(acceptChannel_);
    started_ = true;
    LOG << "WebServer started on port " <<port_;
}

void WebServer::connectionCallback() {
    struct sockaddr_in cliAddr;
    memset(&cliAddr, 0, sizeof(struct sockaddr_in));
    socklen_t len = sizeof(cliAddr);
    int connfd = 0;
    while ((connfd = accept(listenFd_, (struct sockaddr*)&cliAddr, &len)) > 0) {
        EventLoop *loop = reactorPool_->getNextLoop();
        
        // LOG << "New connection from " << inet_ntoa(cliAddr.sin_addr) << ":" << ntohs(cliAddr.sin_port);
        
        if(connfd >= MAXFD) {
            close(connfd);
            continue;
        }
        std::string from(inet_ntoa(cliAddr.sin_addr));
        from.push_back(':');
        from += std::to_string(ntohs(cliAddr.sin_port));

        assert(setNonBlocking(connfd) != -1);
        setNodelay(connfd);
        HttpConnPtr httpConn(new HttpConn(this, loop, connfd, std::move(from)));
        connMap_[connfd] = httpConn;
        loop->queueInLoop(std::bind(&HttpConn::init, httpConn));
    }

    acceptChannel_->setEvent(EPOLLIN | EPOLLET);
    acceptChannel_->update();
}

void WebServer::removeConnInLoop(const HttpConnPtr& conn) {
    loop_->assertInLoopThread();
    // LOG << "WebServer::removeConn in socket fd " << conn->fd();
    connMap_.erase(conn->fd());
}
void WebServer::removeConn(const HttpConnPtr& conn) {
    loop_->queueInLoop(std::bind(&WebServer::removeConnInLoop, this, conn));
}
