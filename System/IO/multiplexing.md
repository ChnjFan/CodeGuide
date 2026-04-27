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

### poll

poll 是 select 的改进版，解决 select 的 fd 数量的限制与位图重复初始化问题。

使用 `pollfd` 结构体数组管理监控 fd 事件，内核填充就绪事件后返回。

```cpp
struct pollfd {
    int fd;         // 待监控的文件描述符
    short events;   // 传入：用户关心的事件（位掩码）
    short revents;  // 传出：内核返回的实际发生事件（位掩码）
};
```

#### 工作原理

调用 `poll` 时，内核遍历整个 `pollfd` 数组检查每个 fd 的事件状态，状态就绪则设置 `revents` 返回就绪总数，否则阻塞在 `poll` 中或超时返回。

poll 默认使用**水平触发模式**，只要 fd 处于就绪状态（如缓冲区有数据）poll 就会返回事件直到数据全部被处理，无边缘触发。

### epoll

epoll 是 Linux 特有的 IO 多路复用技术，解决 select/poll 在高并发下性能差的问题。

- select/poll 每次都要全量遍历所有 fd、重复拷贝数据，并发越高越慢。
- epoll 只返回就绪的 fd，无需重复遍历和重复拷贝。

#### 操作 API

epoll 只有三个系统调用：

- `epoll_create`：创建 epoll 实例（红黑树 + 就绪链表）
- `epoll_ctl`：增 / 删 / 改 要监听的 fd（红黑树操作）
- `epoll_wait`：等待事件返回（只拿就绪链表）

**epoll_create（创建句柄）**

```cpp
#include <sys/epoll.h>
int epoll_create(int size);  // 旧版
int epoll_create1(int flags); // 新版（推荐，flags=0）
```

作用：创建一个 **epoll 专用文件描述符**（唯一标识一个 epoll 对象）。

内部创建两个核心结构：

1. **红黑树**：管理所有被监听的 fd（增删改查极快）
2. **就绪链表**：存放**已经就绪**的 fd（epoll_wait 直接读这里）

**epoll_ctl（操作监听列表）**

```cpp
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
```

**epoll_wait（等待事件）**：阻塞等待就绪事件，返回就绪 fd 的数量。

```cpp
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
```

#### 工作原理

epoll 在内核中通过红黑树管理所有 fd，并将 fd 绑定一个**内核回调函数**。

当网卡收到对端发来的数据后，写入内核 socket 接收缓冲区。硬件触发中断后内核处理网络协议栈，检测到 socket 缓冲区发送变化后出发内核回调函数。

内核回调函数中判断 fd 如果不在就绪链表中，就将 fd 加入 epoll 实例的就绪链表中，然后唤醒阻塞在 `epoll_wait` 的进程。

`epoll_wait` 的进程唤醒后检查就绪链表不会空，拷贝就绪链表中的事件到用户态数组中，返回就绪数量。

用户态程序遍历就绪 fd 集合处理业务。

#### 触发模式

两种触发模式：LT 水平触发和 ET 边缘触发。两种模式的主要区别看**满足就绪条件时，epoll 要不要反复通知**。

##### LT 水平触发

epoll 默认为水平触发，只要 fd 缓冲区处于就绪状态，调用 `epoll_wait` 就会反复持续通知，直到缓冲区数据全部读完，但是只要发送缓冲区没满有空闲，就会持续触发 `EOPLLOUT` 写事件。

epoll 每次调用 `epoll_wait` 都会重新检查 fd 缓冲区状态，只要满足就绪条件就重新加入就绪队列并返回事件。

##### ET 边缘触发

边缘触发只在文件描述符状态发送变化的时候触发一次事件，后续不再重复通知。

- fd 必须设置为非阻塞 `O_NONBLOCK`：阻塞 `read` 缓冲区读完后会阻塞在 `read`，不会继续调用 epoll 事件循环。
- 收到事件后，必须循环读写，直到读完整个缓冲区。
  - 循环读：`read() == -1 && errno == EAGAIN/EWOULDBLOCK`

epoll 内核在 fd 就绪触发回调后，只把 fd 加入就绪队列一次，除非 fd 状态再次发生改变，否则不会重复入队。



