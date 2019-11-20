# WebServer

## Version 0.1

先基本参照muduo和@linyacool的代码复现，主要在这个过程中要想明白每一部分的设计和代码实现。

### EventLoop, Channel, EPoller

\# EventLoop是每个线程里的循环，EPoller用epoll实现对事件I/O事件监听，EventLoop利用EPoller监听事件，并利用Channel回调处理事件。每个Channel负责一个fd，但其不负责关闭fd。Channel的生命周期由所属的EventLoop控制。

\# 搭配EventLoopThreadPool，则构成了一个基本的多Reactor结构。由于HTTP解析处理速度较快，计算较少，故没有再使用工作线程池来处理，这样可以减少线程上下文切换，提高性能。

TODO：测试加入工作线程池能否提升效率。

### 非阻塞I/O，LT/ET

\# 对于监听socket，如果设置为ET，则必须循环调用accpet一次读完，但高并发下可能出现连接失败。如果设置为LT，则可以每次触发事件只读一个请求。

\# 对于已连接socket，LT模式下阻塞与否差不多，为了防止出错一般选择非阻塞。ET模式下必须用非阻塞并且一次读完。

\# 对于LT/ET模式选，在Reactor + 工作线程池的方案中，考虑到不应该将I/O放在多个工作线程，所以应该用ET模式，并在工作线程里读完所有数据。
而在本工程里，当Main Reactor将fd分配给Sub Reactor后，由于I/O等待和处理都是在Sub Reactorr中，故ET模式和LT模式似乎都可以，待后续测试。

TODO：测试ET和LT的效率差别。

### HTTP解析

\# 每个HttpConn负责一个fd，并关联到对应的channel。buffer采用vector<char>，由于存储上表现为char[]，所以比较适配系统I/O调用。

\# 状态机，算是参考最少的部分。在解析过程利用C++ STL库的算法，整个代码相比《Linux高性能服务器编程》里简洁很多。

\# 能处理GET和HEAD请求。

TODO：增加POST，DELETE等请求。

### Timer，超时检查

\# 用timerfd注册到EPoller上作为定时器触发方式。

\# 结构上采用时间堆的方式，即每次以堆顶定时器的时间为触发timerfd可读事件的时间。触发后不断出队直到下一个有效定时器。

\# 采用逻辑删除，即到时后发现删除标志有效则不触发回调函数。

\# 对于HTTP请求启用超时检查，即一段时间没有收到新请求则主动关闭连接。

\# Timer管理不是线程安全的。但因为设计是每个EventLoop持有一个TimerHeap，所以所有操作都在一个线程里，所以目前不会出问题。

TODO：必要的话处理线程安全。

### LOG

\# LogFile里封装对对应日志文件的操作。

\# AsyncLogging里按照muduo的设计方式，将日志分为前后端，前端写入日志，后端一个单独进程处理存入日志文件，用缓冲区交换的方式减小临界区，提高效率。
（发现linyacool的代码没有处理AsyncLogging指针，程序退出前的一小段日志会缺失，memory leakage？用std::shared\_ptr替代裸指针解决）

\# LogStream用操作符重载，让日志写入可以用<<运算符，符合std::cout的使用习惯。

\# Logger里包含了一个LogStream，并利用宏定义，每次用LOG<<时生成Logger临时对象，并在析构时通过AsyncLogging发送至后端。

TODO: 1、日志文件的滚动 2、日志分级


## TODO: 重新过一边代码，检查是否存在内存泄露情况；学习nginx和redis，看看是否有改进方向；完成每一块的TODO。
