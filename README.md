# WebServer

### Build

    /build.sh

```
cd build
./WebServer [port] [threads_number]
```

### 2019-11-22:

0.1版正式完成。解决了第一次写的时候的内存泄露、对象生命周期意外延长等问题。

第一版核心部分虽然具体代码实现不一样，但结构上主要都是muduo的设计(照猫画虎)，HTTP的解析和超时管理部分才比较有我自己的想法。

希望后续再从nginx学到一些东西。

### 2019-12-10

在4核CPU，4G内存的虚拟机Ubuntu中，利用ApacheBench进行了压测，测试结果见imgs目录。

分别用4线程(进程)下的muduo、nginx以及本项目实现简单的GET请求，从内存直接响应一个“Hello World”。分别测试了keep-alive和close选项。

利用本机测试1000并发，200000个请求下的表现。 

    ab -n 200000 -c 1000 -m GET [-k] URL

测试过程不严谨，各个项目响应头的大小不一致，响应内容也很简单，日志也没完全关闭。

没有对比的目的，仅仅是看一个并发性能上的大致表现。

(muduo短连接出现了failed request?)

#### TODO: 

#### 学习nginx，看看是否有改进方向。

#### 学习mysql，结合数据库。

#### 完善文档。
