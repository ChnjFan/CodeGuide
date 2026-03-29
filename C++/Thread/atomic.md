# 原子操作

原子操作是不可再分、不可中断的最小执行单元。整个操作过程不可分割、不可中断，没有中间状态，要么完全执行完毕，要么完全没执行，不会被线程调度或硬件中断打断。

[TOC]

## 整型变量赋值

**整型变量赋值为什么不是原子操作？**

现代 CPU 架构里没有内存直连内存的硬件通路没，整型变量赋值需要借助寄存器中转才能完成内存拷贝，对应两条计算机指令：将数据从内存中读取到寄存器然后再写回内存。

```assembly
mov eax, dword ptr [b]  
mov dword ptr [a], eax
```

两条指令再多线程执行时，某个线程可能再第一条指令执行完毕后被剥夺 CPU 时间片，切换到另一个线程而产生不确定的情况。

## std::atomic

`std::atomic` 是**C++11 引入的原子模板类**，核心作用是**实现多线程环境下的原子操作**，是并发编程的基础组件。

```cpp
template<class T>
struct atomic;
```

注意：`std::atomic` 将拷贝构造删除，因此无法通过 `std::atomic<int> value = 99;` 初始化。

```cpp
std::atomic<int> value = 99; // error: use of deleted function ‘std::atomic<int>::atomic(const std::atomic<int>&)’
std::atomic<int> value_success;
value_success.store(99);
value_success = 99;
```

常用核心操作：

- `store(val)`：原子写入值
- `load()`：原子读取值
- `fetch_add(n)`：原子加法（返回旧值）
- `compare_exchange_weak`：CAS（比较并交换，无锁编程核心）