# 条件变量

条件变量是让线程等待某个条件成立，不成立就休眠避免占用 CPU，条件满足时就通知唤醒线程。由于条件被多个线程操作，因此判断条件是否满足前需要进行加锁操作，判断完毕进行解锁操作。

常用于生产者消费者模型、线程池任务队列和等待某个时间发生的场景。

[TOC]

## Linux 条件变量

在 Linux 系统中 `pthread_cond_t` 即是条件变量的类型。

```cpp
int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr);
int pthread_cond_destroy(pthread_cond_t* cond);

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
```

等待条件变量：

```cpp
int pthread_cond_wait(pthread_cond_t* restrict cond, pthread_mutex_t* restrict mutex);
int pthread_cond_timedwait(pthread_cond_t* restrict cond, pthread_mutex_t* restrict mutex, const struct timespec* restrict abstime);
```

唤醒等下条件变量的线程：

```cpp
int pthread_cond_signal(pthread_cond_t* cond); // 唤醒一个线程
int pthread_cond_broadcast(pthread_cond_t* cond); // 唤醒所有线程
```

生产者消费者模型：

```cpp
void* consumer_thread(void* param)
{	
	Task* pTask = NULL;
	while (true)
	{
		pthread_mutex_lock(&mymutex);
		while (tasks.empty())
		{				
			//如果获得了互斥锁，但是条件不合适的话，pthread_cond_wait会释放锁，不往下执行。
			//当发生变化后，条件合适，pthread_cond_wait将直接获得锁。
			pthread_cond_wait(&mycv, &mymutex);
		}
		
		pTask = tasks.front();
		tasks.pop_front();

		pthread_mutex_unlock(&mymutex);
		
		if (pTask == NULL)
			continue;

		pTask->doTask();
		delete pTask;
		pTask = NULL;		
	}
	
	return NULL;
}

void* producer_thread(void* param)
{
	int taskID = 0;
	Task* pTask = NULL;
	
	while (true)
	{
		pTask = new Task(taskID);
			
		pthread_mutex_lock(&mymutex);
		tasks.push_back(pTask);
		std::cout << "produce a task, taskID: " << taskID << ", threadID: " << pthread_self() << std::endl; 
		
		pthread_mutex_unlock(&mymutex);
		
		//释放信号量，通知消费者线程
		pthread_cond_signal(&mycv);
		
		taskID ++;

		//休眠1秒
		sleep(1);
	}
	
	return NULL;
}
```

## 虚假唤醒

线程在没有收到 `notify` 或 `pthread_cond_signal` 通知却唤醒，此时如果代码中没有再次检查条件就会直接往下执行。

操作系统底层机制决定：

- 内核处理信号时会强制唤醒阻塞的线程
- POSIX 标准允许无通知唤醒
- 系统为了性能效率会批量唤醒导致多余唤醒

虚假唤醒无法避免，需要在唤醒后再次判断条件。

## 信号丢失问题

线程还没有开始调用 `wait()` 等待唤醒信号就已经发完了，导致线程永远也等不到通知。

- 条件变量的 `signal` / `notify` **不记录历史**，只唤醒**当前正在等待**的线程。
- 如果线程 A 先 `signal`，线程 B 后 `wait`，B 收不到之前的信号，会**一直阻塞**。

解决关键**必须用锁 + 共享变量一起判断**，不能只靠信号：

- `wait` 前**先判断条件**，条件已满足就不再等待
- 把 “条件判断 + wait” 做成原子操作

## std::condition_variable

`std::condition_variable` 是 **C++11 标准的条件变量**，让一个线程**等待某个条件成立**，在等待期间**休眠、不占 CPU**，直到其他线程通知它醒来。

- **必须配合 `std::unique_lock` 使用**（不能用 lock_guard /shared_lock）
- `wait()` 会自动：**解锁 → 休眠 → 唤醒 → 重新加锁**
- *必须判断条件**（防止**虚假唤醒**）
- `notify_one()` / `notify_all()` 用来唤醒等待线程

```cpp
// 等待条件成立（自动解锁+休眠+重锁）
wait(unique_lock<mutex>& lock);
wait(unique_lock<mutex>& lock, 谓词条件);

// 超时等待
wait_for(...);
wait_until(...);

// 唤醒一个等待线程
notify_one();

// 唤醒所有等待线程
notify_all();
```

`wait()` 内部首先会自动解锁然后休眠线程，等到被唤醒后重新加锁，检查条件是否成立。

因为要自动加解锁，所以 `lock_guard` 不能使用。

```cpp
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

std::mutex mtx;
std::condition_variable cv;
bool ready = false;

void worker() {
    std::unique_lock<std::mutex> lock(mtx);
    
    // 等待 ready == true
    cv.wait(lock, []{ return ready; });

    std::cout << "线程被唤醒！\n";
}

int main() {
    std::thread t(worker);

    std::lock_guard<std::mutex> lock(mtx);
    ready = true;
    cv.notify_one();  // 唤醒

    t.join();
    return 0;
}
```

