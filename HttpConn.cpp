#include "HttpConn.h"

#include <iostream>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include "net/EventLoop.h"
#include "net/SQLConnection.h"
#include "WebServer.h"
#include "net/SocketUtils.h"

const int initBuffSize = 1024;
const char HttpConn::CRLF[] = "\r\n";
const int defautlEvent = EPOLLIN | EPOLLOUT | EPOLLET;
const int defaultTimeout = 8 * 1000;
const long keepAliveTimeout =  90 * 1000;
const std::string HttpConn::root = get_current_dir_name();
int HttpConn::idCnt = 0;

HttpConn::HttpConn(WebServer* server, int fd) 
    : id_(idCnt++),
      server_(server),
      channel_(new Channel(nullptr, fd)),
      fd_(fd), 
      connStatus_(Connected),
      readBuff_(initBuffSize),
      read_idx_(0),
      checked_idx_(0), 
      writeBuff_(initBuffSize),
      write_idx_(0),
      send_idx_(0),
      linger_(false),
      parseState_(CheckRequestLine),
      readable_(true), 
      writeable_(false) {
}

HttpConn::HttpConn(WebServer* server, EventLoop* loop, int fd, std::string from) 
    : id_(idCnt++),
      server_(server),
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
      parseState_(CheckRequestLine),
      readable_(true), 
      writeable_(false) {
}

HttpConn::~HttpConn() { 
}

void HttpConn::init(EventLoop* loop, std::string from, int fd) {
    connStatus_ = Connected;
    fd_ = fd;
    channel_->reset(loop, fd);
    loop_ = loop;
    loop_->addConnCnt();
    from_ = std::move(from);
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
    std::vector<char> t1(initBuffSize);
    std::vector<char> t2(initBuffSize);
    readBuff_.swap(t1);
    writeBuff_.swap(t2);
    headers_.clear();
    outputHeaders_.clear();
    body_.clear();
    readable_ = true;
    writeable_ = false;
    channel_->setEvent(defautlEvent);
}

void HttpConn::close() {
    connStatus_ = Disconnected;
    loop_->removeChannel(channel_);
    channel_->clearAll();
    ::close(fd_); 
    reset();
    loop_->clearTimer(id_);
    loop_->minusConnCnt();
    linger_ = false;
    fd_ = -1;
    server_->returnConn(shared_from_this());
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
        if (writeable_) 
            write();
        else {
            loop_->runInLoop(std::bind(&HttpConn::close, this));
            return;
        }
        
    }
    else {
        read_idx_ += readBytes;
        StatusCode c = parseRequest();
        if (c != Wait) {
            handleResponse(c);
        } else {
            channel_->update();
        }
    }
    
    if (connStatus_ == Connected) {
        if (linger_) 
            loop_->addTimer(id_, keepAliveTimeout, std::bind(&HttpConn::close, this)); 
        else 
            loop_->addTimer(id_, defaultTimeout, std::bind(&HttpConn::close, this));   
    }
    // handleConn();
}

void HttpConn::write() {
    if (writeable_ && write_idx_ != send_idx_) {
        int writeBytes;
        if ((writeBytes = writen(fd_, writeBuff_.data() + send_idx_, write_idx_ - send_idx_)) < 0) {
            LOG << "Http Response Error: HttpConn::write()";
        } else {
            send_idx_ += writeBytes;
        }
    }

    if (parseState_ == Finished && write_idx_ == send_idx_) {
        if (connStatus_ == Connected && linger_) {
            reset();
            channel_->update();
        } else {
            channel_->setEvent(0);
            channel_->update();
            loop_->runInLoop(std::bind(&HttpConn::close, this));
            return;
        }
    }
    //handleConn();
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
        pos = requestLine.find("POST");
        if (pos >= 0) {
            method_ = Post;
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
        requestBody_ = std::string(readBuff_.data() + checked_idx_, readBuff_.data() + checked_idx_ + contentLen);
        checked_idx_ += contentLen;
        return OK;
    } 
    else return Wait;
}

HttpConn::StatusCode HttpConn::processRequest() {
    if (headers_.find("Connection") != headers_.end()) {
        if (headers_["Connection"] == "keep-alive" || headers_["Connection"] == "Keep-Alive") {
            linger_ = true;
        } 
    }
    outputHeaders_["Connection"] = linger_ ? "keep-alive" : "close";
    if (method_ == Head) {
        LOG << "Processing HEAD request from " << from_;
        return OK;
    } else if (method_ == Get) {
        if (path_ == "/hello") {
            body_ = "Hello World";
            outputHeaders_["Content-Type"] = "text/plain";
            return OK;
        }
        if (path_ == "/")
            path_ = "/index.html";
        // LOG << "Processing GET request from " << from_ << ": get " << path_;
        return handleFile(path_);
    } else if (method_ == Post) {
        if (path_ == "/signup" || path_ == "/login") {
            
#if 0
            std::shared_ptr<SQLConnection> sql = loop_->sqlConnection();
            if (!sql) {
                body_ = "Inernal Error!";
                return InternalError;
            }
            
            outputHeaders_["Content-Type"] = "text/plain";
            std::string name = findInBody("username");
            std::string pwd = findInBody("password");
            
            if (name.empty() || pwd.empty()) {
                body_ = "Wrong Body";
                return OK;
            }

            bool exist = server_->existUser(name);
            if (path_ == "/signup") {
                if (exist) {
                    body_ = "User already exist!";
                    return OK;
                }
                std::string query("INSERT INTO `user` VALUES(");
                query += "'" + name + "', '" + pwd + "')";
                if (sql->sqlQuery(query)) {
                    server_->updateUser(name, pwd);
                    body_= "Sign up OK!";
                } else {
                    body_ = "Failed!";
                    return OK;
                }
            }
            if (path_ == "/login") {
                if (server_->verifyUser(name, pwd)) {
                    body_ = "Login OK!";
                } else {
                    body_ = "Wrong user or password";
                    return OK;
                }
            }
#endif
            return OK;
        }
        return BadRequest;

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
    header += "Server: Xiaojy\r\n";
    outputHeaders_["Content-Length"] = std::to_string(body_.size());
    for (auto p : outputHeaders_) {
        header += p.first + ": " +p.second +"\r\n";
    }
    header += "\r\n";
    while (writeBuff_.size() - write_idx_ < header.size() + body_.size()) writeBuff_.resize(writeBuff_.size() * 2);
    std::copy(header.begin(), header.end(), writeBuff_.data() + write_idx_);
    write_idx_ += header.size();
    std::copy(body_.begin(), body_.end(), writeBuff_.data() + write_idx_);
    write_idx_ += body_.size();
    writeable_ = true;
}


int HttpConn::findCRLF(int start) const {
    auto beg = readBuff_.begin();
    auto it = std::search(beg + start, beg + read_idx_, CRLF, CRLF + 2);
    return it == readBuff_.end() ? -1 : it - beg;
}

std::string HttpConn::findInBody(const std::string& key) {
    size_t idx = requestBody_.find(key);
    if (idx == requestBody_.npos) return "";
    idx += key.size() + 1;
    size_t s_idx = idx;
    idx = requestBody_.find('&', idx);
    if (idx == requestBody_.npos) return requestBody_.substr(s_idx);
    return requestBody_.substr(s_idx, idx - s_idx);
}

HttpConn::StatusCode HttpConn::handleFile(const std::string& filepath) {
    std::string path = root +"/.." + filepath;
    struct stat fileStat;
    if (stat(path.c_str(), &fileStat) < 0) {
        handleFile("/404.html");
        return NotFound;
    }
    if (!(fileStat.st_mode & S_IROTH)) {
        handleFile("/403.html");
        return Forbidden;
    }
    if (S_ISDIR(fileStat.st_mode)) {
        handleFile("/400.html");
        return BadRequest;
    }
    int fd = open(path.c_str(), O_RDONLY);
    void* mmapAddr = mmap(0, fileStat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    ::close(fd);
    if (mmapAddr == (void*)-1) {
        munmap(mmapAddr, fileStat.st_size);
        handleFile("/400.html");
        return BadRequest;
    }

    char* file = static_cast<char*>(mmapAddr);
    body_ = std::string(file, file + fileStat.st_size);
    handleType(filepath);
    return OK;
}

void HttpConn::handleType(const std::string& filepath) {
    if (filepath.find(".html") != filepath.npos) {
        outputHeaders_["Content-Type"] = "text/html; charset=utf-8";
    } else if (filepath.find(".jpg") != filepath.npos) {
        outputHeaders_["Content-Type"] = "image/jpg";
    } else if (filepath.find(".ico") != filepath.npos) {
        outputHeaders_["Content-Type"] = "image/x-icon";
    } else if (filepath.find(".css") != filepath.npos) {
        outputHeaders_["Content-Type"] = "text/css";
    } else {
        outputHeaders_["Content-Type"] = "text/plain";
    }
}
