# WebServer

### Introduction

* Linux下的简易Web服务器，采用C++编写。

* 使用Linux下的epoll作为I/O多路复用技术，采用非阻塞+边沿触发的方式。

* 使用多线程Reactor模型，主Reactor负责accept，子Reactor负责I/O事件，通过事件循环，对read和write派发。

* 采用固定数量的线程池，每个线程一个Reactor。

* 使用eventfd进行跨线程调用和线程的异步唤醒。

* 使用timerfd注册到epoll中作为定时器触发方式。

* 使用手动实现的可调整的小根堆实现定时器的管理。

* 使用双缓冲区的异步日志系统，用一个专门的线程负责日志写入。

* 使用状态机解析Http请求，能处理GET，HEAD，POST请求。

* 用链表实现连接池，避免连接到来和断开时，创建销毁对象的开销。

* GET请求经测试能处理html、css、jpg、ico等。

* POST请求搭配mysql，实现了简单的用户注册、登陆功能。

* 使用RAII技术，减少内存泄漏。

### Requirements

* Linux >= 2.6.28

* GCC >= 4.7

### Build and Usage

    ./build.sh

```
cd build
./WebServer [port] [threads_number]
```

### [Model](https://github.com/Raven65/WebServer/blob/master/MODEL.md)

### Performance

测试工具: Apache Bench

测试环境: 虚拟机Ubuntu 18.04, 4 CPUs, 4 GB Memory

响应内容: 内存中的"Hello World"字符串

以下测试时没有连上mysql，根据测试，保持mysql连接对性能有一定影响。

PS: nginx 的Response Header要大的多，所以虽然QPS更小，但从Transfer Rate比较还是nginx更优秀。结果可见imgs里的图。

PPS: 后来又使用了WebBench和http\_load测试，发现用三个测试工具结果差别很大，在WebBench测试时本项目表现比muduo和nginx差了很多，但http\_load测试时又和用AB时一样本项目是表现最好的，应该和客户端实现方式有关，下面结果只针对ApacheBench。

![avatar](https://github.com/Raven65/WebServer/blob/master/imgs/Performance%20close.png)

![avatar](https://github.com/Raven65/WebServer/blob/master/imgs/Performance%20keep-alive.png)
