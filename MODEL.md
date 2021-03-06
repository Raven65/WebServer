### 并发模型

项目使用Reactor模型，有固定数量的I/O线程作为Sub Reactor，每个线程都使用epoll作为I/O复用方式。

主线程只负责处理监听listenfd，而Sub Reactor负责建立连接后的I/O工作。此外还有一个LOG线程进行日志写入。目前不设置另外的工作线程。

### EventLoop, Channel, EPoller

这三个核心结构是模仿muduo的设计实现，不过比muduo实际代码作了简化。

实现one loop per thread的设计，每个I/O线程里都有一个sub reactor进行事件的分发，main reactor负责listen和accept套接字，然后通过其中一个I/O线程处理新连接的I/O事件。

当SubReactor在epoll\_wait阻塞时，MainReactor通过linux的eventfd特性唤醒子线程，处理新连接事件。

模仿muduo使用了runInLoop和queueInLoop的设计，queueInLoop类似nginx的post event设计，在完成一次epoll返回事件的处理后才调用注册的函数。
当A线程向B线程添加新事件时，注册到B事件的事件队列中，这时锁征用只发生在A和B之间。MainReactor就是通过这个方式在SubReactor中注册处理新建立的连接的，再配合eventfd将SubReactor唤醒。

EventLoop是每个线程里的循环，EPoller用epoll实现对事件I/O事件监听，并根据发生事件的fd激活Channel，记录返回events。

每个Channel负责一个fd，但其不负责关闭fd。Channel的生命周期由事件消费者(即HTTP模块)控制，并由事件消费者在Channel中注册回调。项目中几乎所有回调都用std::bind实现。

EventLoop中根据Channel的revents调用注册好的回调函数。

搭配EventLoopThreadPool，构建线程池，避免创建开销。

由于静态HTTP解析处理速度较快，计算较少，故没有再使用工作线程池来处理，这样可以减少线程上下文切换和不必要的锁争用，提高性能。

### 非阻塞I/O，LT/ET

对于监听socket，如果设置为ET，则必须循环调用accpet一次读完，但高并发下可能出现连接失败。如果设置为LT，muduo的做法是每次触发事件只读一个请求，循环触发。

对于已连接socket，LT模式下阻塞与否差不多，为了防止出错一般选择非阻塞。ET模式下必须用非阻塞并且一次读完。

对于LT/ET模式，在Reactor + 工作线程池的方案中，考虑到不应该将I/O放在多个工作线程，所以应该用ET模式，并在工作线程里读完所有数据。

而在本工程里，当Main Reactor将fd分配给Sub Reactor后，由于I/O等待和处理都是在Sub Reactorr中，故ET模式和LT模式应该差不多，主要是编程上的差别。网上查阅的资料有的说，在超高并发下这才会成为性能瓶颈，否则差不了太多。

由于一开始是从《Linux高性能服务器编程》开始入手的，本项目选择的是ET模式。

### HTTP业务

每个HttpConn负责一个fd，并关联到对应的channel。buffer采用vector<char>，由于存储上表现为char[]，所以比较适配系统I/O调用。

状态机，算是参考最少的部分。在解析过程利用C++ STL库的算法，整个代码相比《Linux高性能服务器编程》里简洁很多。

处理GET和HEAD请求。

模仿nginx，预先创建了HTTP连接池，避免频繁的创建和销毁对象的开销，利用智能指针来保证生命周期。

用链表存储空闲连接，如果链表中有空闲连接则直接取，没有就新建一个连接，用完后先从epoll中移除事件，然后关闭fd，再做处理加入到空闲连接链表。

实现了POST请求，并简单利用mysql做了一个用户注册、登陆功能的测试。

TODO：mysql连接池，调通cgi。

### Timer，超时检查

用timerfd注册到EPoller上作为定时器触发方式。

结构上采用时间小顶堆的方式，即每次以堆顶定时器的时间为触发timerfd可读事件的时间。触发后不断弹出堆直到下一个有效定时器。

手写实现了时间堆，主要是因为STL的堆操作都不支持修改和删除的动态调整堆。逻辑删除的作法虽然可行，但是高并发下会一直创建对象，内存会膨胀，而且每次会把时间浪费在弹出那些被逻辑删除的定时器中。

利用map中存储定时器的weak\_ptr，堆中存储shared\_ptr的作法，可以根据一个标志id快速定位定时器。由于现在定时器只是用于超时检查没有其他用途，所以定时器map的key现在都是HttpConn::id，较好的实现应该对每一个定时事件都生成一个id。

由于timerfd需要搭配clock\_gettime使用，而频繁的系统调用效率很低，学习了libevent的时间缓存作法，在loop开始时重置标志，而后第一次取时间会更新缓存，相当于一次loop里只有一个时间，牺牲了精度，提高了效率。

对于HTTP请求启用超时检查，即一段时间没有收到数据则主动关闭连接。

目前上述操作都不是线程安全的。但因为设计是每个EventLoop持有一个TimerHeap，各个线程定时器独立，所以所有操作都在一个线程里，目前不会出问题。

### LOG

LogFile里封装对对应日志文件的操作。

AsyncLogging里按照muduo的设计方式，将日志分为前后端，前端写入日志，后端一个单独进程处理存入日志文件，用缓冲区交换的方式减小临界区，提高效率。

LogStream用操作符重载，让日志写入可以用<<运算符，符合std::cout的使用习惯。

Logger里包含了一个LogStream，并利用宏定义，每次用LOG<<时生成Logger临时对象，并在析构时通过AsyncLogging发送至后端。

TODO: 1、日志文件的滚动 2、日志分级
