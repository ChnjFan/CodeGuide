# 互斥锁

**互斥锁**是 C++ 多线程编程中**最基础、最核心**的同步工具，专门解决**多线程同时修改共享数据**导致的**数据竞争、结果错乱、程序崩溃**问题。

[TOC]

## Linux 互斥锁

Linux 互斥锁实现在原生 POSIX 线程库，定义在 `<pthread.h>` 头文件中。

```c
#include <pthead.h>
// 使用 PTHREAD_MUTEX_INITIALIZER 初始化，不需要手动释放
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
// 动态分配或给互斥锁设置属性，attr 默认属性设置为 NULL
int pthread_mutex_init(pthread_mutex_t* restrict mutex, 
					   const pthread_mutexattr_t* restrict attr);
// 动态分配的互斥锁需要手动释放
int pthread_mutex_destroy(pthread_mutex_t* mutex);
```

销毁已经加锁或正在被条件变量使用的互斥锁，执行函数会返回错误。

互斥锁加解锁操作：

```c
int pthread_mutex_lock(pthread_mutex_t* mutex);
int pthread_mutex_trylock(pthread_mutex_t* mutex);// 非阻塞，立即返回结果
int pthread_mutex_unlock(pthread_mutex_t* mutex);
```

互斥锁对象属性 `pthread_mutexattr_t` 使用前需要初始化，使用后要销毁。

```c
int pthread_mutexattr_init(pthread_mutexattr_t* attr);
int pthread_mutexattr_destroy(pthread_mutexattr_t* attr);
int pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type);
int pthread_mutexattr_gettype(const pthread_mutexattr_t* restrict attr, int* restrict type);
```

常用属性类型有：

- `PTHREAD_MUTEX_NORMAL`：默认属性普通锁，加锁后会阻塞在加锁调用处直到对互斥锁加锁的线程释放了锁。
- `PTHREAD_MUTEX_ERRORCHECK`：检错锁，同一个线程如果对已经加锁的互斥锁对象再次加锁，会立即返回加锁失败错误。如果是多个线程加锁则不会报错。
- `PTHREAD_MUTEX_RECURSIVE`：嵌套锁，允许同一进程对持有的互斥锁重复加锁，每次调用加锁引用计数就会增加一次，调用解锁后引用计数会减少一次，直到锁的引用计数为 0 才会允许其他线程获得锁。

## 读写锁

实际应用中，大部分情况是线程只是读取共享变量的值，并不修改，只有极少数情况下线程才会真正修改值。

读写锁使用类型 `pthread_rwlock_t` 表示。

```c
#include <pthread.h>

pthread_rwlock_t myrwlock = PTHREAD_RWLOCK_INITIALIZER;
// 动态创建读写锁
int pthread_rwlock_init(pthread_rwlock_t* rwlock, const pthread_rwlockattr_t* attr);
int pthread_rwlock_destroy(pthread_rwlock_t* rwlock);
// 请求读锁
int pthread_rwlock_rdlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_tryrdlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_timedrdlock(pthread_rwlock_t* rwlock, const struct timespec* abstime);
// 请求写锁
int pthread_rwlock_wrlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_trywrlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_timedwrlock(pthread_rwlock_t* rwlock, const struct timespec* abstime);
// 释放锁
int pthread_rwlock_unlock (pthread_rwlock_t* rwlock);
```

- 读锁用于共享模式，当读写锁被读模式占有，其他线程请求读锁会立刻获得锁，请求写锁则会陷入阻塞。

- 写锁用于独占模式，当读写锁被写模式占用，无论调用读锁请求还是写锁请求，都会陷入阻塞直到线程释放写锁。

读写锁的属性可以设置读锁优先还是写锁优先，在多个线程同时请求锁时决定哪个线程优先获得锁。

## std::mutex

C++11 及以后版本提供了完整了互斥锁库。

| 工具                          | 版本  | 作用                                           |
| ----------------------------- | ----- | ---------------------------------------------- |
| **`std::mutex`**              | C++11 | 基础互斥锁，手动 lock/unlock                   |
| **`std::timed_mutex`**        | C++11 | 有超时机制的互斥锁                             |
| **`std::recursive_mutex`**    | C++11 | 递归锁，同一线程可多次加锁                     |
| **`std::shared_timed_mutex`** | C++14 | 具有超时机制的共享互斥锁                       |
| **`std::shared_mutex`**       | C++17 | 共享互斥锁，读写分离，优化**读多写少**并发场景 |

为了避免死锁，互斥锁的加锁、解锁方法要成对使用，通常使用 RAII 进行封装。

| 互斥量管理           | 版本  | 作用                   |
| :------------------- | :---- | :--------------------- |
| **std::lock_guard**  | C++11 | 基于作用域的互斥量管理 |
| **std::unique_lock** | C++11 | 更加灵活的互斥量管理   |
| **std::shared_lock** | C++14 | 共享互斥量的管理       |
| **std::scope_lock**  | C++17 | 多互斥量避免死锁的管理 |