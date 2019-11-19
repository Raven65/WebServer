#pragma once

#include <memory>
#include <vector>
#include <string>
#include <map>
#include "Channel.h"
#include "base/noncopyable.h"

class WebServer;

class HttpConn : noncopyable, public std::enable_shared_from_this<HttpConn> {
public:
    enum ConnStatus { Disconnected, Connected, Disconnecting };
    enum Method { Get = 1, Post, Head, Put, Delete };
    enum Version { Http10 = 1, Http11 };
    enum StatusCode { Wait, OK = 200, BadRequest = 400, Forbidden = 403, NotFound = 404, InternalError = 500 };
    enum ParseState { CheckRequestLine, CheckHeaders, CheckBody, Finished };
    
    HttpConn(WebServer* server, EventLoop* loop,  int fd, std::string from);
    ~HttpConn();
    
    int fd() { return fd_; }

    void init();
    void reset();
    void close();
    void read();
    void write();

    
private:
    StatusCode parseRequest();

    StatusCode parseRequesetLine(int end_idx);
    StatusCode parseHeader();
    StatusCode parseBody();

    StatusCode processRequest();

    void handleResponse(StatusCode code);
    void handleConn();

private:
    WebServer* server_;
    EventLoop* loop_;
    ChannelPtr channel_;
    int fd_;
    std::string from_;
    ConnStatus connStatus_; 

    std::vector<char> readBuff_;
    int read_idx_;
    int checked_idx_;
    std::vector<char> writeBuff_;
    int write_idx_;
    int send_idx_;
    int findCRLF(int start) const;

    bool linger_;
    std::string body_;
    std::map<std::string, std::string> headers_;
    ParseState parseState_;
    Method method_;
    std::string path_;
    Version version_;
    static const char CRLF[];

    static const std::string root;
};

typedef std::shared_ptr<HttpConn> HttpConnPtr;
