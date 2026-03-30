# 信号量

Linux 的信号量（Semaphore）是**用于实现进程 / 线程间同步与互斥**的**IPC（进程间通信）机制**，核心是通过一个计数器和等待 / 唤醒操作，控制多个执行单元对共享资源的访问。本质是指资源存在多份，可以同时被多个线程访问。

[TOC]

## 核心操作

信号量本质是一个**非负整数计数器**，记录可用资源的数量。

核心操作是**P 操作（申请资源）\**和\**V 操作（释放资源）**，且这两个操作必须是**原子操作**（不可被中断，确保多进程 / 线程下的安全性）。

```c
#include <semaphore.h>
int sem_init(sem_t* sem, int pshared, unsigned int value);
int sem_destroy(sem_t* sem);
int sem_post(sem_t* sem);
int sem_wait(sem_t* sem);
int sem_trywait(sem_t* sem);
int sem_timedwait(sem_t* sem, const struct timespec* abs_timeout);
```

- `sem_init` 用于初始化信号量。参数 `pshared` 表示信号量是否可以被多个进程之间共享（0：一个进程多个线程共享，非 0：多个进程间共享）。参数 `value` 表示资源初始数量。
- `sem_post` 将信号量资源计数加 1，解锁信号量对象，其他使用 `sem_wait` 被阻塞的线程被唤醒。
- 信号量资源计数为 0 时，调用 `sem_wait` 会阻塞线程，直到信号量对象的资源计数大于 0 时被唤醒。

