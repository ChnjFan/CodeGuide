# Redis 网络通信模块

Redis 网络通信模块采用**单线程 Reactor + I/O 多路复用**架构，核心是 ae 事件库、anet 网络封装、RESP 协议与客户端管理。

[TOC]

## 整体架构

Redis 4.0 网络模块是**单线程事件驱动**的核心，负责：

- 监听与接受客户端连接（TCP/Unix Socket）
- 读写网络数据、解析 / 序列化 RESP 协议
- 管理客户端状态与连接生命周期
- 驱动整个服务的事件循环（aeEventLoop）

单线程 Reactor 模型（核心）

- **主线程唯一**：所有网络 IO、命令执行、定时任务都在一个线程
- **事件驱动**：无轮询，仅在 fd 就绪时处理
- **读优先**：先处理读事件，保证请求不堆积
- **O (1) 事件访问**：用 fd 直接索引 `events` 数组，无哈希表开销

## 核心组件

### 事件驱动核心：ae 库（ae.c/ae.h）

ae 是 Redis 自研的**跨平台 I/O 多路复用事件库**，统一封装底层系统 API，是网络模块的 “心脏”。

根据不同平台实现不同的多路复用方式，Linux 在 `ae_epoll.c` 实现 epoll 接口。

```cpp
static int aeApiPoll(aeEventLoop *eventLoop, struct timeval *tvp) {
    aeApiState *state = eventLoop->apidata;
    int retval, numevents = 0;
	// 根据是否存在定时器事件来设置超时事件，-1 不会超时唤醒直到有时间发生
    retval = epoll_wait(state->epfd,state->events,eventLoop->setsize,
            tvp ? (tvp->tv_sec*1000 + tvp->tv_usec/1000) : -1);
    if (retval > 0) {
        int j;

        numevents = retval;
        for (j = 0; j < numevents; j++) {
            int mask = 0;
            struct epoll_event *e = state->events+j;
			// 将事件都记录在事件循环对象 aeEventLoop 中
            if (e->events & EPOLLIN) mask |= AE_READABLE;
            if (e->events & EPOLLOUT) mask |= AE_WRITABLE;
            if (e->events & EPOLLERR) mask |= AE_WRITABLE;
            if (e->events & EPOLLHUP) mask |= AE_WRITABLE;
            eventLoop->fired[j].fd = e->data.fd;
            eventLoop->fired[j].mask = mask;
        }
    }
    return numevents;
}
```

### 网络操作封装：anet 库（anet.c/anet.h）

anet 是**跨平台 socket 工具库**，封装 TCP/Unix Socket 的创建、监听、连接、设置等，屏蔽系统差异。

### 协议层：RESP（Redis Serialization Protocol）

Redis 4.0 使用 **RESP2** 作为通信协议，文本友好、易于解析、高性能。

## 事件循环流程

**初始化**：`initServer` 创建监听 socket、注册 accept 事件、初始化 ae 循环

**启动循环**：`aeMain` → 循环调用 `aeProcessEvents`

**aeProcessEvents 核心逻辑**

- 调用 `aeApiPoll`（如 `epoll_wait`）阻塞等待就绪事件

- 遍历 `fired` 数组，按 **读优先** 执行回调

  - 监听 fd 可读 → `acceptTcpHandler`（新连接）
  - 客户端 fd 可读 → `readQueryFromClient`（读请求）
  - 客户端 fd 可写 → `sendReplyToClient`（写响应）

  

- 处理定时事件（如 `serverCron`）

**命令执行**：`readQueryFromClient` 解析命令 → `processCommand` → 执行命令 → `addReply` 写入响应

**响应发送**：写事件触发 → `sendReplyToClient` 发送数据

### 初始化流程

`initServer` 是服务启动的核心初始化函数，主要创建监听 socket、事件循环和定时任务等的初始化。

#### 网络监听

在 `_anetTcpServer` 中创建 socket 并监听端口 6379。

```cpp
static int _anetTcpServer(char *err, int port, char *bindaddr, int af, int backlog)
{
    int s = -1, rv;
    char _port[6];  /* strlen("65535") */
    struct addrinfo hints, *servinfo, *p;

    snprintf(_port,6,"%d",port);
    memset(&hints,0,sizeof(hints));
    hints.ai_family = af;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;    /* No effect if bindaddr != NULL */
	// getaddrinfo既可以解析IPv4地址，也可以解析IPv6地址
    if ((rv = getaddrinfo(bindaddr,_port,&hints,&servinfo)) != 0) {
        anetSetError(err, "%s", gai_strerror(rv));
        return ANET_ERR;
    }
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((s = socket(p->ai_family,p->ai_socktype,p->ai_protocol)) == -1)
            continue;

        if (af == AF_INET6 && anetV6Only(err,s) == ANET_ERR) goto error;
        if (anetSetReuseAddr(err,s) == ANET_ERR) goto error;
        if (anetListen(err,s,p->ai_addr,p->ai_addrlen,backlog) == ANET_ERR) s = ANET_ERR;
        goto end;
    }
    if (p == NULL) {
        anetSetError(err, "unable to bind socket, errno: %d", errno);
        goto error;
    }

error:
    if (s != -1) close(s);
    s = ANET_ERR;
end:
    freeaddrinfo(servinfo);
    return s;
}
```

#### 创建事件监听句柄

`epollfd` 是**Linux 下 epoll 模型的文件描述符**，管理所有需要监控的 socket / 文件描述符的 IO 事件，通过 `epoll_create()` 创建。

```cpp
void initServer(void) {
       
    //记录程序进程 ID   
    server.pid = getpid();
      
    //创建程序的 aeEventLoop 对象和 epfd 对象
    server.el = aeCreateEventLoop(server.maxclients+CONFIG_FDSET_INCR);

    //创建侦听 fd
    listenToPort(server.port,server.ipfd,&server.ipfd_count) == C_ERR)
         
	//创建 Redis 的定时器，用于执行定时任务 cron
	/* Create the timer callback, this is our way to process many background
     * operations incrementally, like clients timeout, eviction of unaccessed
     * expired keys and so forth. */
    aeCreateTimeEvent(server.el, 1, serverCron, NULL, NULL) == AE_ERR
	
	//将侦听 fd 绑定到 epfd 上去
    /* Create an event handler for accepting new connections in TCP and Unix
     * domain sockets. */
     aeCreateFileEvent(server.el, server.ipfd[j], AE_READABLE, acceptTcpHandler,NULL) == AE_ERR
    
	//创建一个管道，用于在需要时去唤醒 epoll_wait 挂起的整个 EventLoop
    /* Register a readable event for the pipe used to awake the event loop
     * when a blocked client in a module needs attention. */
    aeCreateFileEvent(server.el, server.module_blocked_pipe[0], AE_READABLE, moduleBlockedClientPipeReadable,NULL) == AE_ERR)
}
```



### 接受客户端连接

初始化完成 socket 监听后，调用 `aeMain` 函数启动一个循环不断处理事件。

```cpp
void aeMain(aeEventLoop *eventLoop) {
    eventLoop->stop = 0;
    while (!eventLoop->stop) {
        if (eventLoop->beforesleep != NULL)
            eventLoop->beforesleep(eventLoop);
        aeProcessEvents(eventLoop, AE_ALL_EVENTS|AE_CALL_AFTER_SLEEP);
    }
}
```

事件处理中，根据 `flags` 检查要处理的事件，对于存在定时器事件，先要获取最近到期的定时器，然后在 `aeApiPoll` 中设置阻塞时间。

事件处理的核心流程：

```cpp
int aeProcessEvents(aeEventLoop *eventLoop, int flags)
{
    // 检查事件标记 flags
    // 获取最近到期的定时器
    shortest = aeSearchNearestTimer(eventLoop);
    // 调用多路复用API，设置超时事件
    numevents = aeApiPoll(eventLoop, tvp);
	// 处理事件
    for (j = 0; j < numevents; j++) {
        aeFileEvent *fe = &eventLoop->events[eventLoop->fired[j].fd];
        // 通常先处理读时间再处理写事件，这样在处理完查询请求后能立即响应返回查询结果
        // 设置了AE_BARRIER标记后可以先处理写事件再处理读事件
        int invert = fe->mask & AE_BARRIER;
        
        if (!invert && fe->mask & mask & AE_READABLE) {
            fe->rfileProc(eventLoop,fd,fe->clientData,mask);
            fired++;
        }
        /* Fire the writable event. */
        if (fe->mask & mask & AE_WRITABLE) {
            if (!fired || fe->wfileProc != fe->rfileProc) {
                fe->wfileProc(eventLoop,fd,fe->clientData,mask);
                fired++;
            }
        }
        /* If we have to invert the call, fire the readable event now
         * after the writable one. */
        if (invert && fe->mask & mask & AE_READABLE) {
            if (!fired || fe->wfileProc != fe->rfileProc) {
                fe->rfileProc(eventLoop,fd,fe->clientData,mask);
                fired++;
            }
        }
        processed++;
    }
    /* Check time events */
    if (flags & AE_TIME_EVENTS)
        processed += processTimeEvents(eventLoop);

    return processed; /* return the number of processed file/time events */
}
```

