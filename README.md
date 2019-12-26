# WebServer

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

\# nginx 的Response Header要大的多，所以虽然QPS更小，但从Transfer Rate比较还是nginx更优秀。结果可见imgs里的图。

![avatar](https://github.com/Raven65/WebServer/blob/master/imgs/Performance%20close.png)

![avatar](https://github.com/Raven65/WebServer/blob/master/imgs/Performance%20keep-alive.png)
