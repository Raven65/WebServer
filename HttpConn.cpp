#include "HttpConn.h"

#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include "EventLoop.h"
#include "WebServer.h"
#include "SocketUtils.h"


const int initBuffSize = 1024;
const char HttpConn::CRLF[] = "\r\n";
const int defautlEvent = EPOLLIN | EPOLLET | EPOLLONESHOT;
const std::string root = "/home/xiaojy/project/WebServer/html";


HttpConn::HttpConn(WebServer* server, EventLoop* loop, int fd, std::string from) 
    : server_(server),
      loop_(loop),
      channel_(new Channel(loop, fd)),
      fd_(fd), 
      from_(std::move(from)),
      connStatus_(Connected),
      readBuff_(initBuffSize),
      read_idx_(0),
      checked_idx_(0), 
      writeBuff_(initBuffSize),
      write_idx_(0),
      send_idx_(0),
      linger_(false),
      parseState_(CheckRequestLine){

}

HttpConn::~HttpConn() { LOG << "Close connection from " << from_; ::close(fd_); }

void HttpConn::init() {
    channel_->setReadCallback(std::bind(&HttpConn::read, this));
    channel_->setWriteCallback(std::bind(&HttpConn::write, this));
    channel_->setCloseCallback(std::bind(&HttpConn::close, this));
    channel_->setEvent(defautlEvent);
    loop_->addChannel(channel_);
}

void HttpConn::reset() {
    path_.clear();
    parseState_ = CheckRequestLine;
    read_idx_ = checked_idx_ = 0;
    write_idx_ = send_idx_ = 0;
    headers_.clear();
    body_.clear();
}

void HttpConn::read() {
    if (readBuff_.size() - read_idx_ == 0) readBuff_.resize(readBuff_.size() * 2);
    int readBytes = readn(fd_, readBuff_.data() + read_idx_, readBuff_.size() - read_idx_);
    if (readBytes < 0) {
        body_ = "Read Socket Error!";
        handleResponse(BadRequest);
    } else if (readBytes == 0) {
        connStatus_ = Disconnecting;
        if (parseState_ != Finished) {
            body_ = "Request is not complete!";
            handleResponse(BadRequest);
        } 
    } else {
        read_idx_ += readBytes;
        StatusCode c = parseRequest();
        if(c != Wait) handleResponse(c);
    }
    handleConn();
}

void HttpConn::write() {
    if (connStatus_ != Disconnected) {
        int writeBytes;
        if ((writeBytes = writen(fd_, writeBuff_.data() + send_idx_, write_idx_ - send_idx_)) < 0) {
            LOG << "Http Response Error: HttpConn::write()";
        } else {
            send_idx_ += writeBytes;
        }
    }
    handleConn();    
}

void HttpConn::close() {
    connStatus_ = Disconnected;
    HttpConnPtr guard(shared_from_this());
    loop_->removeChannel(channel_);
    server_->removeConn(guard);    
}

HttpConn::StatusCode HttpConn::parseRequest() {
    StatusCode code;
    bool hasMore = true;
    while (hasMore) {
    
        switch (parseState_) {
        case CheckRequestLine: 
        {
            int idx = findCRLF(checked_idx_);
            if (idx != -1) {
                code = parseRequesetLine(idx);
                if (code == OK){ 
                    parseState_ = CheckHeaders; 
                }
                else if (code == BadRequest) { 
                    body_ = "Error Request Line!";
                    return BadRequest; 
                }
            } else {
                hasMore = false;
            }
            break;
        }
        case CheckHeaders:
        {
            code = parseHeader();
            if (code == OK) {
                parseState_ = CheckBody;
            } else if (code == BadRequest) {
                body_  = "Error Header!";
                return BadRequest;
            }
            else hasMore = false;
            break;
        }
        case CheckBody:
        {
            code = parseBody();
            if (code == OK) {
                parseState_ = Finished;
                return processRequest();
            } 
            if (code != Wait)
                hasMore = false;
            break;
        }
        case Finished: hasMore = false; break;

    }

    }
    return Wait;
}

HttpConn::StatusCode HttpConn::parseRequesetLine(int end_idx) {
    std::string requestLine(readBuff_.data(), readBuff_.data() + end_idx);
    auto beg = requestLine.begin();
    int pos;
    do {
        pos = requestLine.find("GET");
        if (pos >= 0) {
            method_ = Get;
            checked_idx_ += 4;
            break;
        }
        pos = requestLine.find("HEAD");
        if (pos >= 0) {
            method_ = Head;
            checked_idx_ += 5;
            break;
        }
        //加入其他方法
        return BadRequest;
    } while (false);
    
    pos = requestLine.find(' ', checked_idx_);
    if (pos < 0) {
        return BadRequest;
    } else {
        path_.assign(beg + checked_idx_, beg + pos);
        checked_idx_ = pos + 1;
    }
   
    if ((requestLine.size() - checked_idx_) != 8 || requestLine.find("HTTP/1.", checked_idx_) < 0) {
        return BadRequest;
    } else if (requestLine.back() == '1') {
        version_ = Http11;
    } else if (requestLine.back() == '0') {
        version_ = Http11;
    } else 
        return BadRequest;
    
    checked_idx_ = requestLine.size() + 2;
    return OK;
}

HttpConn::StatusCode HttpConn::parseHeader() {
    bool hasMore = true;
    bool ok = false;
    while (hasMore) {
        int pos = findCRLF(checked_idx_);
        if (pos >= 0) {
            const char* beg = readBuff_.data() + checked_idx_;
            const char* crlf = readBuff_.data() + pos;
            const char* colon = std::find(readBuff_.data() + checked_idx_, readBuff_.data() + pos, ':');
            if (colon != crlf) {
                std::string key(beg, colon);
                ++colon;
                while (colon < crlf && isspace(*colon)) ++colon;
                while (colon < crlf && isspace(*(crlf-1))) --crlf;
                std::string value(colon, crlf);
                headers_[key] = value;
            } else {
                hasMore = false;
                ok = true;
            }
            checked_idx_ = pos + 2; 
        } else {
            hasMore = false;
        }
    }
    if (ok) return OK;
    else return Wait;
}

HttpConn::StatusCode HttpConn::parseBody() {

    int contentLen = headers_["Content-Length"].empty() ? 0 : std::stoi(headers_["Content-Length"]);
    if (read_idx_ >= contentLen + checked_idx_) {
        checked_idx_ += contentLen;
        return OK;
    } 
    else return Wait;
}

HttpConn::StatusCode HttpConn::processRequest() {
    if (headers_.find("Connection") != headers_.end()) {
        if(headers_["Connection"] == "keep-alive" || headers_["Connection"] == "Keep-Alive")
            linger_ = true;
    }
    if (method_ == Head) {
        LOG << "Processing HEAD request from " << from_;
        return OK;
    } else if (method_ == Get) {
        path_ = ::root + path_;
        LOG << "Processing GET request from " << from_ << ": get" << path_;
        struct stat fileStat;
        if (stat(path_.c_str(), &fileStat) < 0) {
            body_ = "File does not exist!";
            return NotFound;
        }
        if (!(fileStat.st_mode & S_IROTH)) {
            body_ = "Permission denied!";
            return Forbidden;
        }
        if (S_ISDIR(fileStat.st_mode)) {
            body_ = "It is a directory!";
            return BadRequest;
        }
        int fd = open(path_.c_str(), O_RDONLY);
        void* mmapAddr = mmap(0, fileStat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        ::close(fd);
        if (mmapAddr == (void*)-1) {
            munmap(mmapAddr, fileStat.st_size);
            body_ = "File can't be opened!";
            return BadRequest;
        }

        char* file = static_cast<char*>(mmapAddr);
        body_ = std::string(file, file + fileStat.st_size);
        return OK;
    }
    return BadRequest;
}

void HttpConn::handleResponse(StatusCode code) {
    std::string header;
    header += "HTTP/1.1 ";
    header += std::to_string(code);
    switch(code) {
        case OK: header += " OK"; break;
        case BadRequest: header += " Bad Request"; break;
        case Forbidden: header += " Forbidden"; break;
        case NotFound: header += " Not Found"; break;
        case InternalError: header+= " Internal Error"; break;
        case Wait: break;
    } 
    header += "\r\n";
    header += "Content-Length: " + std::to_string(body_.size()) + "\r\n";
    header += std::string("Connection: ") + (linger_ ? "keep-alive" : "close") + "\r\n";
    header += "\r\n";
    while (writeBuff_.size() - write_idx_ < header.size() + body_.size()) writeBuff_.resize(writeBuff_.size() * 2);
    std::copy(header.begin(), header.end(), writeBuff_.data() + write_idx_);
    write_idx_ += header.size();
    std::copy(body_.begin(), body_.end(), writeBuff_.data() + write_idx_);
    write_idx_ += body_.size();
}

void HttpConn::handleConn() {
    int event = channel_->events();
    if (connStatus_ == Connected) {
        if (parseState_ != Finished) {
            channel_->setEvent(defautlEvent);
        } else {
            if(send_idx_ != write_idx_) event |= EPOLLOUT;
            else {
                event &= ~EPOLLOUT;
                reset();
            }


            if (linger_) {
                event |= defautlEvent;
                //TODO: 定时心跳
                //TODO: 重置状态
            } else {
                
            }
            channel_->setEvent(event);
        }
    }
    else if (connStatus_ == Disconnecting) {
        if(send_idx_ != write_idx_)
            channel_->setEvent (EPOLLOUT | EPOLLET);
        else {
            channel_->setEvent(0);
            loop_->runInLoop(std::bind(&HttpConn::close, shared_from_this()));
            return;
        }
    }
    channel_->update();
}

int HttpConn::findCRLF(int start) const {
    auto beg = readBuff_.begin();
    auto it = std::search(beg + start, beg + read_idx_, CRLF, CRLF + 2);
    return it == readBuff_.end() ? -1 : it - beg;
}