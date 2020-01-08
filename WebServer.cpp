#include "WebServer.h"
#include <arpa/inet.h>
#include <functional>
#include <unistd.h>
#include <sys/epoll.h>
#include "net/SocketUtils.h"
#include "net/SQLConnection.h"
#include "base/Logger.h"

WebServer::WebServer(EventLoop* loop, int threadNum, int port)
    : loop_(loop),
      threadNum_(threadNum),
      reactorPool_(new EventLoopThreadPool(loop_, threadNum_)),
      started_(false),
      port_(port),
      connPool_(loop, this, MAXFD >> 2) {
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

    sql = std::make_shared<SQLConnection>();
    if (sql->initConnection("127.0.0.1", "root", "12345678", "WebServer", 3306)) {
        std::string query("CREATE TABLE `user` (");
        query += "`username` varchar(255) COLLATE utf8mb4_unicode_520_ci NOT NULL,";
        query += "`password` varchar(255) COLLATE utf8mb4_unicode_520_ci NOT NULL,";
        query += "PRIMARY KEY (`username`)";
        query += ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_520_ci;";
        sql->sqlQuery(query);
        
        std::string query1("SELECT * FROM `user`");
        if (sql->sqlQuery(query1)) {
            while (MYSQL_ROW row = sql->fetchRow()) {
                users[row[0]] = users[row[1]];
            }
        }
    } else{
        sql.reset();
    }
}

void WebServer::start() {
    reactorPool_->start();
    connPool_.start();
    ignoreSigpipe();    
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
        HttpConnPtr httpConn = connPool_.getNextConn();
        loop->runInLoop(std::bind(&HttpConn::init, httpConn, loop, std::move(from), connfd));
    }

    acceptChannel_->setEvent(EPOLLIN | EPOLLET);
    acceptChannel_->update();
}

void WebServer::returnConnInLoop(HttpConnPtr conn) {
    loop_->assertInLoopThread();
    connPool_.addFreeConn(std::move(conn));
}
void WebServer::returnConn(HttpConnPtr conn) {
    loop_->runInLoop(std::bind(&WebServer::returnConnInLoop, this, std::move(conn)));
}
void WebServer::updateUser(const std::string& name, const std::string& pwd) {
    
    {
        MutexLockGuard lock(mutex_);        
        users[name] = pwd;
    }
    std::string query("INSERT INTO `user` VALUES('");
    query += name + "', '" + pwd +"');";
    if (sql) sql->sqlQuery(query);
}
