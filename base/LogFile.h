#pragma once

#include <string>
#include <memory>
#include "MutexLock.h"
#include "noncopyable.h"

//not thread safe
class AppendFile : noncopyable {
public:
    explicit AppendFile(std::string filename);
    ~AppendFile() { fclose(fp_); }
    
    void append(const char* logline, size_t len);
    void flush() { fflush(fp_); }
    off_t writtenBytes() const { return writtenBytes_; }

private:
    size_t write(const char* logline, size_t len);
    FILE* fp_;
    char buffer_[64*1024];
    off_t writtenBytes_;
};

//TODO: ROLL LOGFILE
class LogFile : noncopyable {
public:
    LogFile(const std::string& basename, int flushEveryN = 1024);
    ~LogFile() = default;
    
    void append(const char* logline, int len);
    void flush();
    bool rollFile();

private:
    void append_unlocked(const char* logline, int len);

    const std::string basename_;
    const int flushEveryN_;

    int count_;
    std::unique_ptr<MutexLock> mutex_;
    std::unique_ptr<AppendFile> file_;

};
