# 高性能异步 IO 框架：io_uring

`io_uring` 是 Linux 内核 5.1 版本（2019 年）引入的新一代异步 I/O 框架，旨在解决传统异步 I/O（如 `aio_read/aio_write`）的性能瓶颈、API 不统一、功能受限等问题。

[TOC]

## 传统 IO 的困境

同步 IO 发起系统调用的线程会被阻塞，直到内核完成 IO 操作并返回结果。

- **线程阻塞导致资源闲置**：阻塞期间线程无法处理其他请求，CPU 核心被限制，线程自身仍占用内存和内核资源。如果系统有 10K 个并发 IO 请求，同步模型需要创建 10K 个线程处理，内存开销急剧膨胀。
- **上下文切换开销**：IO 完成后内核需要将阻塞的线程从等待队列唤醒，触发线程上下文切换（1~10微秒/次），高并发场景上下文切换开销会占满 CPU 资源。

传统  POSIX AIO（以下简称「传统 AIO」）是 Linux 早期的异步 I/O 实现，仅支持磁盘文件的直接 IO（O_DIRECT），无法作为统一的异步框架。

> O_DIRECT：直接与磁盘交互，不通过系统页缓存。默认文件的鞋模式是非 O_DIRECT，依赖操作系统的页缓存实现数据中转，读写操作先经过页缓存而非直接读写磁盘。

传统 AIO 性能差，系统调用开销大，内核处理效率低：

- **系统调用次数多**：单个请求需调用 `aio_read()`/`aio_write()`（系统调用），获取结果需 `aio_error()`+`aio_return()`（多次系统调用）。
- **内核处理效率低**：内核需为每个请求创建内核线程（`kworker`），线程上下文切换开销大。
- **实现复杂**：API 设计存在缺陷。
  - 请求和结果需要手动维护 `struct aiocb` 结构体。
  - 错误处理需要先调用 `aio_error()` 检查是否完成，再调用 `aio_return()` 获取结果。
  - 等待结果需使用 `aio_suspend()`（仅支持信号量 / 超时）或 `sigevent` 信号回调（信号处理逻辑复杂，易引发竞态）。
  - 

## io_uring 核心设计

io_uring 通过共享内存的**环形队列**和**批量处理**来解决传统异步 IO 的性能瓶颈和功能受限等问题。

- **环形队列**：用户态和内核态共享两个环形队列——提交队列（SQ）和完成队列（CQ）。用户向 SQ 写入请求，内核从 SQ 读取并处理，完成后的结果写入 CQ，整个流程避免系统调用。
- **批量处理**：多个 IO 请求可以一次性提交，多个事件可以一次性获取，减少了用户态和内核态的切换次数。

### 工作流程

`io_uring` 基于两个核心环形队列实现，环形队列基于共享内存（mmap 内核空间）实现，用户态到内核态无需拷贝数据，直接访问。

环形队列通过索引计数器同步，无锁设计。

- 提交队列 SQ：用户态向内核提交 IO 请求的环形队列，内核读取。
- 完成队列 CQ：内核向用户态返回 IO 结果的环形队列，用户读取。

工作流程如下：

![](../Pics/io_uring_work.png)

1. 用户态通过 `io_uring_get_sqe()` 获取空闲 SQE，填充参数后，通过 `io_uring_submit()` 将 SQE 提交到 SQ；
2. 内核触发读取 SQ：
   - 主动触发：调用 `io_uring_enter()` 系统调用，通知内核处理 SQ；
   - 被动触发：启用 SQ_POLL 模式（内核线程轮询 SQ），无需主动调用；
3. 用户态从 CQ 获取结果：
   - 轮询：`io_uring_peek_cqe()` 非阻塞检查 CQ；
   - 阻塞：`io_uring_wait_cqe()` 阻塞等待 CQ 有结果；
   - 事件驱动：结合 `epoll` 监听 CQ 就绪事件（内核 5.8+ 支持）。

### 核心特性

#### SQPOLL 模式

内核通过创建 SQ_POLL 线程，持续轮询 SQ 是否有新请求。用户态提交 SQE 后，内核线程立即处理，无需用户态调用 `io_uring_enter()` 通知内核。

消除提交请求的系统调用开销，优化性能，在创建 `io_uring` 时设置`IORING_SETUP_SQPOLL` 标志。

```cpp
#include <liburing.h>

struct io_uring ring;
struct io_uring_params params;
params.flags = IORING_SETUP_SQPOLL;
int ret = io_uring_queue_init_params(1024, &ring, &params);
```

- IORING_SETUP_IOPOLL：忙轮询设备就绪状态代替中断，优化低延迟场景 IO 性能。
- IORING_SETUP_SINGLE_ISSUER：限制 IO 提交者只能为单一线程或进程，降低内核同步开销，优化单线程提交场景。



