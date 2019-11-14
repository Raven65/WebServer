#include "Logger.h"

#include <assert.h>
#include <time.h>
#include <sys/time.h>
#include <memory>
#include "Thread.h"
#include "AsyncLogging.h"

static pthread_once_t once_control = PTHREAD_ONCE_INIT;
static std::shared_ptr<AsyncLogging> AsyncLogger_;

std::string Logger::logFileName_ = "./WebServer.log";

void once_init() {
    AsyncLogger_ = std::shared_ptr<AsyncLogging>(new AsyncLogging(Logger::getLogFileName()));
    AsyncLogger_->start();
}

void output(const char* msg, int len) {
    pthread_once(&once_control, once_init);
    AsyncLogger_->append(msg, len);
}

Logger::Impl::Impl(const char* fileName, int line)
    : stream_(),
      line_(line),
      basename_(fileName) {
    formatTime();
}

void Logger::Impl::formatTime() {
    struct timeval tv;
    time_t time;
    char str_t[26] = {0};
    gettimeofday(&tv, NULL);
    time = tv.tv_sec;
    struct tm* p_time = localtime(&time);
    strftime(str_t, 26, "%Y-%m-%d %H:%M:%S  ", p_time);
    stream_ << str_t;
}

Logger::Logger(const char* fileName, int line) : impl_(fileName, line) {}

Logger::~Logger() {
    impl_.stream_ << " -- " << impl_.basename_ << ":" << impl_.line_ << "\n";
    const LogStream::Buffer& buf(stream().buffer());
    output(buf.data(), buf.length());
}
