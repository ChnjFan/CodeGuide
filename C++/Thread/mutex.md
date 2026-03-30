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

`std::mutex` 是**C++11 标准库**（`<mutex>` 头文件）提供的**基础互斥锁**，是多线程编程的核心同步工具。

多个线程同时读写**共享资源**（变量、数据结构）时，通过锁保证**同一时间只有一个线程**能访问该资源，避免数据错乱。

| 工具                          | 版本  | 作用                                             |
| ----------------------------- | ----- | ------------------------------------------------ |
| **`std::mutex`**              | C++11 | 基础互斥锁，手动 lock/unlock                     |
| **`std::timed_mutex`**        | C++11 | 有超时机制的互斥锁，超时如果还没有拿到锁直接返回 |
| **`std::recursive_mutex`**    | C++11 | 递归锁，同一线程可多次加锁                       |
| **`std::shared_timed_mutex`** | C++14 | 具有超时机制的共享互斥锁                         |
| **`std::shared_mutex`**       | C++17 | 共享互斥锁，读写分离，优化**读多写少**并发场景   |

### 核心操作

1. `lock()`：加锁，锁被占用则**阻塞等待**，直到获取锁；
2. `unlock()`：解锁，释放锁让其他线程获取；
3. `try_lock()`：非阻塞尝试加锁，成功返回`true`，失败直接返回`false`。
4. **`try_lock_for(时间段)`**：尝试加锁，等待**指定时长**（如 100 毫秒），超时未拿到则返回 `false`；
5. **`try_lock_until(时间点)`**：尝试加锁，等待**到指定时间点**（如 2025-05-20 10:00:00），超时返回 `false`；

### 互斥锁包装器

为了避免死锁，互斥锁的加锁、解锁方法要成对使用，通常使用 RAII 进行封装。

| 互斥量管理           | 版本  | 作用                   |
| :------------------- | :---- | :--------------------- |
| **std::lock_guard**  | C++11 | 基于作用域的互斥量管理 |
| **std::unique_lock** | C++11 | 更加灵活的互斥量管理   |
| **std::shared_lock** | C++14 | 共享互斥量的管理       |
| **std::scope_lock**  | C++17 | 多互斥量避免死锁的管理 |

`std::lock_guard` 是 C++ 标准库提供的**RAII 机制互斥锁包装器**，核心作用是**自动管理互斥锁（`std::mutex`）的加锁和解锁**，避免手动操作锁导致的死锁、资源泄漏问题。

```cpp
#include <mutex>
std::mutex mtx;
void func() {
    std::lock_guard<std::mutex> lock(mtx);  // 构造：自动加锁
    // 临界区代码（线程安全的共享资源操作） 
} // 离开作用域：析构，自动解锁
```

`std::unique_lock` 除了自动解锁外，支持延迟加锁、手动解锁、尝试加锁和锁所有权转移。

```cpp
// ✅ 手动提前解锁，后续代码不占用锁
std::unique_lock<std::mutex> lock(mtx);  // 构造时自动加锁
shared_data++;  // 临界区操作
lock.unlock();
// 🔒 构造时【不自动加锁】（第二个参数：std::defer_lock）
std::unique_lock<std::mutex> lock(mtx, std::defer_lock);
// 先执行无锁的准备工作（不操作共享数据）
prepare_data();  
// ✅ 需要时手动加锁
lock.lock();   
// 操作共享数据
lock.unlock(); 
```

`std::shared_lock` 是 C++14 引入的**共享锁（读锁）RAII 包装器**，专为 `std::shared_mutex` / `std::shared_timed_mutex` 设计，核心是**允许多线程并发读、写操作必须独占**，专门解决**读多写少**场景的并发性能问题。

```cpp
#include <shared_mutex>
#include <thread>
#include <vector>
#include <iostream>

std::shared_mutex shm;
int shared_data = 0;

// 读线程：用 shared_lock，多线程可同时读
void reader(int id) {
    std::shared_lock<std::shared_mutex> lock(shm); // 自动 lock_shared()
    std::cout << "Reader " << id << " read: " << shared_data << "\n";
    // 离开作用域自动 unlock_shared()
}

// 写线程：用 unique_lock，独占访问
void writer(int value) {
    std::unique_lock<std::shared_mutex> lock(shm); // 自动 lock()
    shared_data = value;
    std::cout << "Writer updated to: " << value << "\n";
}

int main() {
    std::vector<std::thread> readers;
    for (int i=0; i<5; ++i) readers.emplace_back(reader, i);
    std::thread w(writer, 42);
    for (auto& t : readers) t.join();
    w.join();
    return 0;
}
```

`std::scoped_lock` 是 **C++17 引入的 RAII 锁守卫**，核心是**同时安全锁定多个互斥量、自动防死锁、作用域结束自动解锁**，是多锁场景的首选。

多线程同时操作多个共享资源时，手动加锁易因顺序不一致死锁，`scoped_lock` 自动解决。

```cpp
#include <mutex>
#include <thread>

std::mutex mtx1, mtx2;
int data1 = 0, data2 = 0;

void safe_operation() {
    // ✅ 同时锁定 mtx1、mtx2，自动防死锁
    std::scoped_lock lock(mtx1, mtx2);
    data1++;
    data2++;
    // 离开作用域自动逆序解锁
}

int main() {
    std::thread t1(safe_operation);
    std::thread t2(safe_operation);
    t1.join(); t2.join();
    return 0;
}
```

## 经验总结

### 减少锁的使用

实际开发中应该尽量减少锁的使用，使用锁产生的性能损耗主要在：

- 加锁和解锁操作
- 临界区代码无法并发执行
- 进入临界区过于频繁，线程间对临界区争夺激烈，在 CPU 执行上下文切换消耗

替代锁的方式：无锁队列。

### 明确锁的范围

明确加锁的资源边界，精确锁定最小作用域。

```cpp
if(hashtable.is_empty())
{
    pthread_mutex_lock(&mutex);
    htable_insert(hashtable, &elem);
    pthread_mutex_unlock(&mutex);
}
```

`is_empty` 条件判断完后在加锁前状态改变，这时候仍执行条件内的操作，会导致结果于代码表达的意思不一致。

```cpp
pthread_mutex_lock(&mutex);
if(hashtable.is_empty())
{
    htable_insert(hashtable, &elem);
}
pthread_mutex_unlock(&mutex);
```

### 减少锁的粒度

减小锁使用粒度指的是尽量减小锁作用的临界区代码范围，临界区的代码范围越小，多个线程排队进入临界区的时间就会越短。

```cpp
void TaskPool::addTask(Task* task)
{
    std::shared_ptr<Task> spTask;
    spTask.reset(task);

    {
        std::lock_guard<std::mutex> guard(m_mutexList);             
        m_taskList.push_back(spTask);
    }
    
    m_cv.notify_one();
}

void EventLoop::doPendingFunctors()
{
	std::vector<Functor> functors;
	
	{// 通过局部变量保存共享数据 pendingFunctors_ 减少临界区
		std::unique_lock<std::mutex> lock(mutex_);
		functors.swap(pendingFunctors_);
	}

	for (size_t i = 0; i < functors.size(); ++i)
	{
		functors[i]();
	}	
}
```

### 避免死锁

- 函数加锁操作，在函数退出前要记得解锁。
- 线程退出一定要及时释放持有的锁，线程结束锁不会自动释放。
- 多线程请求锁的方向要一致，避免死锁。
- 同一线程重复请求一个锁，要搞清楚锁的行为，是递增引用计数，还是会阻塞或直接获得锁。

### 避免活锁

活锁是在多个线程使用 `trylock()` 系列函数，由于多个线程互相谦让，导致某段时间锁资源可用也让需要锁的线程拿不到锁。

实际编码中要尽量避免，不要过多使用 `trylock()` 请求锁。
