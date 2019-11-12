#include "LogFile.h"

#include <assert.h>
#include <stdio.h>

AppendFile::AppendFile(std::string filename) : fp_(::fopen(filename.c_str(), "ae")), writtenBytes_(0) {
    assert(fp_);
    setbuffer(fp_, buffer_, sizeof buffer_);
}

void AppendFile::append(const char* logline, const size_t len) {
    size_t n = write(logline, len);
    size_t remain = len - n;
    while (remain > 0) {
        size_t x = write(logline + n, remain);
        if (x == 0) {
           int err = ferror(fp_);
           if (err) {
               fprintf(stderr, "AppendFile::append() failed!\n");
               break;
           }
        }
        n += x;
        remain = len - n;
    }
    writtenBytes_ += len;
}

size_t AppendFile::write(const char* logline, size_t len) {
    return fwrite_unlocked(logline, 1, len, fp_);
}

LogFile::LogFile(const std::string& basename, int flushEveryN)
    : basename_(basename), flushEveryN_(flushEveryN), count_(0), mutex_(new MutexLock) {
    assert(basename_.find('/') >= 0);
    file_.reset(new AppendFile(basename));
}

void LogFile::append(const char* logline, int len) {
    MutexLockGuard lock(*mutex_);
    append_unlocked(logline, len);
}

void LogFile::flush() {
    MutexLockGuard lock(*mutex_);
    file_->flush();
}

void LogFile::append_unlocked(const char* logline, int len) {
    file_->append(logline, len);
    ++count_;
    if (count_>=flushEveryN_) {
        count_ = 0;
        file_->flush();
    }
}
