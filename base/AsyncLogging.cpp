#include "AsyncLogging.h"
#include <assert.h>
#include <functional>
#include "LogFile.h"

AsyncLogging::AsyncLogging(std::string logName, int flushInterval) 
    : flushInterval_(flushInterval),
      running_(false),
      basename_(logName),
      thread_(std::bind(&AsyncLogging::threadFunc, this), "Logging"),
      mutex_(),
      cond_(mutex_), 
      currentBuffer_(new Buffer),
      buffers_(),
      latch_(1) {
    assert(logName.size() > 1);
    currentBuffer_->bzero();
    buffers_.reserve(16);
}

void AsyncLogging::append(const char* logline, int len) {
    MutexLockGuard lock(mutex_);
    if (currentBuffer_->avail() > len) {
        currentBuffer_->append(logline, len);
    }
    else {
        buffers_.push_back(std::move(currentBuffer_));
        currentBuffer_.reset(new Buffer);
        currentBuffer_->append(logline, len);
        cond_.notify();
    }
}

void AsyncLogging::threadFunc() {
    assert(running_ == true);
    latch_.countDown();
    LogFile output(basename_);
    BufferPtr newBuffer(new Buffer);
    newBuffer->bzero();
    BufferVector buffersToWrite;
    buffersToWrite.reserve(16);

    while (running_) {
        assert(newBuffer && newBuffer->length() == 0);
        assert(buffersToWrite.empty());
        {
            MutexLockGuard lock(mutex_);
            if(buffers_.empty()) {
                cond_.waitForSeconds(flushInterval_);
            }
            buffers_.push_back(std::move(currentBuffer_));
            currentBuffer_ = std::move(newBuffer);
            buffersToWrite.swap(buffers_);
        }
        
        assert(!buffersToWrite.empty());

        if (buffersToWrite.size() > 25) {
            buffersToWrite.erase(buffersToWrite.begin() + 2, buffersToWrite.end());
        }

        for (const auto& buffer : buffersToWrite) {
            output.append(buffer->data(), buffer->length());
        }

        if (buffersToWrite.size() > 1) {
            buffersToWrite.resize(1); 
        }

        if (!newBuffer) {
            assert(!buffersToWrite.empty());
            newBuffer = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer->reset();
        }

        buffersToWrite.clear();
        output.flush();
    }

    output.flush();
}
