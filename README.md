# WebServer

## Version 0.1

先基本参照muduo和@linyacool的代码复现，主要在这个过程中要想明白每一部分的设计和代码实现。

### EventLoop, Channel, EPoller

### LOG

\#LogFile里封装对对应日志文件的操作。

\#AsyncLogging里按照muduo的设计方式，将日志分为前后端，前端写入日志，后端一个单独进程处理存入日志文件，用缓冲区交换的方式减小临界区，提高效率。
（发现linyacool的代码没有处理AsyncLogging指针，程序退出前的一小段日志会缺失，memory leakage？用std::shared\_ptr替代裸指针解决）

\#LogStream用操作符重载，让日志写入可以用<<运算符，符合std::cout的使用习惯。

\#Logger里包含了一个LogStream，并利用宏定义，每次用LOG<<时生成Logger临时对象，并在析构时通过AsyncLogging发送至后端。


TODO: 1、日志文件的滚动 2、日志分级
