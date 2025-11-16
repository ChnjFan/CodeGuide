# C10K 问题：异步 IO 与多路复用

C10K 问题提过的核心是传统网络服务器架构无法高效处理超过 1 万个并发网络连接，会导致性能急剧下降、响应延迟飙升甚至服务崩溃。

传统的 `read()` 和 `write()` 接口在 CPU 发起 IO 请求后就只能阻塞等待数据准备好，连接越多等待的线程就越多。

解决 C10K 问题的核心是放弃“一个连接一个进程/线程”的模型，使用更高效的 IO 处理机制。

[TOC]

## IO 多路复用机制

I/O 多路复用（I/O Multiplexing）是一种高效的网络编程技术，允许单个进程（或线程）同时同时监视多个 I/O 文件描述符（如网络套接字、本地文件、管道等），并在这些描述符中**任何一个就绪（可读 / 可写 / 发生异常）时**，由内核通知进程进行处理。

### select

`select` 是 **最早的 I/O 多路复用机制**，其核心作用是让单个进程 / 线程同时监视多个 **文件描述符**，其中任意一个 FD 就绪（可读 / 可写 / 发生异常）时，通知进程进行处理。

```c++
#include <sys/select.h>
int select(int nfds, fd_set *_Nullable restrict readfds,
          fd_set *_Nullable restrict writefds,
          fd_set *_Nullable restrict exceptfds,
          struct timeval *_Nullable restrict timeout);
```

#### 核心原理

`select` 的核心是通过 **文件描述符集合**告知内核需监控的 FD，并通过 **内核轮询 + 状态回写** 通知用户态就绪的 FD。

##### 关键数据结构：文件描述符集合（fd_set）

根据函数定义，`select` 要求用户构造 3 个 FD 集合，分别对应可读、可写和异常三个事件，内核通过这三个集合确定需监控的 FD 和事件类型。

`fd_set` 是一个 **固定大小的位图（Bitmask）**，每个比特位对应一个 FD（如比特位 0 对应 FD=0，比特位 1 对应 FD=1，以此类推）。

**为什么用位图？**

位图的优势是 **操作高效**：设置 “监控某个 FD” 只需用位运算（如 `FD_SET(fd, &readfds)`），内核检查 FD 状态也只需判断对应比特位是否为 1。

##### 工作流程

1. **用户态准备 FD 集合**

- `FD_ZERO(&set)`：清空 FD 集合（所有比特位设为 0）；
- `FD_SET(fd, &set)`：将指定 FD 加入集合（对应比特位设为 1）；
- `FD_CLR(fd, &set)`：从集合中移除指定 FD（对应比特位设为 0）；
- `FD_ISSET(fd, &set)`：检查 FD 是否在集合中（判断对应比特位是否为 1）。

```cpp
#include <sys/select.h>
int main() {
    fd_set readfds;       // 可读事件集合
    // 初始化集合
    FD_ZERO(&readfds);
    FD_SET(socket1, &readfds);
    FD_SET(socket2, &readfds);
}
```

2. **内核态监控轮询 FD 状态**

用户程序调用 `select` 后，进程会从用户态切换到内核态，内核轮询检查 FD 状态。

若有 FD 就绪，记录就绪 FD（将 `fd_set` 中的比特位置为 1）并返回就绪 FD 的数量。

无 FD 就绪，将进程挂起直到有 FD 就绪或超时再唤醒，更新 `fd_set` 后返回。

3. **用户态遍历处理**

内核完成监控后，会将更新后的 `fd_set`（仅保留就绪的 FD）回写至用户态，并唤醒进程。用户态需要遍历 FD 集合查看哪些 FD 准备就绪，并执行对应的 IO 操作。

```cpp
int ret = select(max_fd + 1, &readfds, NULL, NULL, NULL);
if (ret > 0) {
    // 遍历所有fd,检查哪些准备好了
    for (int i = 0; i < max_fd; i++) {
        if (FD_ISSET(i, &readfds)) {
            // 这个fd准备好了,可以read()了
            read(i, buf, sizeof(buf));
        }
    }
}
```

#### 核心限制

尽管 `select` 解决了 “多连接管理” 的问题，但存在以下问题导致无法应对 **10 万级（C100K）以上的并发**：

- **FD 数量限制**：`select` 监控的 FD 数量由内核参数 `FD_SETSIZE` 决定（默认 1024），且无法动态调整。
  - `fd_set` 是 **固定大小的位图**，内核在编译时已确定其长度。
- **每次调用复制 FD 集合**：`select` 每次调用时，用户态的 `fd_set` 必须 **完整复制到内核态**；内核处理完成后，又需将更新后的 `fd_set` 复制回用户态。
  - 默认 1024 数量位图占用 128 字节，频繁调用会造成复制开销显著增加。
- **内核轮询所有 FD**：内核每次处理 `select` 时，都会 **从 0 遍历到 max_fd**，无论这些 FD 是否被监控。
  - 用户只监控 FD = 1023，内核仍会遍历 0 ~ 1022 FD。
  - 当 FD 数量达到 100K 后，内核每次轮询的时间复杂度为 O(n)，无效遍历过多。