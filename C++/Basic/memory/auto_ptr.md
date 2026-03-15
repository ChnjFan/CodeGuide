# 智能指针

[TOC]

## RAII

RAII（资源获取即初始化）是 C++ 管理资源的核心思想，将资源的生命周期与对象的生命周期绑定。

- 资源在对象构造时获取初始化；
- 资源在对象析构时释放；
- 只要对象出了作用域，析构函数一定会被执行，保证资源被正确释放。

RAII 解决手动管理资源存在的资源忘记释放，函数抛出异常后释放资源代码无法执行，重复释放资源或野指针问题等。

### 原理分析

**1、RAII 在异常场景下为什么能保证资源释放？**

C++ 在异常处理时，栈上的局部对象会依次销毁，析构函数执行时资源被正确释放。

**2、为什么 RAII 类通常要禁用拷贝构造和赋值运算符？**

避免多个 RAII 对象管理同一份资源，析构时同时释放资源会导致资源重复释放。例如智能指针管理裸指针，如果多个对象管理同一个内存地址，对内存重复释放。

### 应用场景

`std::lock_guard` 管理互斥锁：

```cpp
class LockGuard {
public:
    explicit LockGuard(std::muatex& mtx) : mtx_(mtx) {
        mtx_.lock();// 加锁，如果其他线程已经加锁则阻塞在这里
    }
    ~LockGuard() noexcept {
        mtx_.unlock();// 析构时解锁
    }
private:
    std::mutex& mtx_;
    
    // 通常禁用拷贝构造和赋值运算符，防止多个对象共用一把锁
    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;
};
```

## 智能指针

智能指针是根据 RAII 封装裸指针，管理内存申请释放，避免手动申请释放造成的内存泄露、重复释放和野指针等问题。

### std::unique_ptr

`std::unique_ptr` 是独占式智能指针。

- 独占所有权：同一时间只有一个 `unique_ptr` 能指向同一块内存，禁止拷贝（拷贝构造 / 赋值运算符被 `delete`）。
- 移动语义：支持 `std::move` 转移指针所有权。
- 轻量级：无额外内存开销，效率与裸指针几乎一致。
- 支持自定义删除器：可释放非内存资源，可以管理文件句柄、数组等其他资源。

```cpp
template <typename T>
class UniquePtr {
public:
    explicit UniquePtr(T* ptr = nullptr) : ptr_(ptr) {}
    ~UniquePtr() noexcept {
        if (!ptr) {
            delete ptr;
        }
    }
    
    UniquePtr(UniquePtr&& other) : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }
    UniquePtr& operator=(UniquePtr&& other) {
        if (this != &other) {
            delete ptr_;
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }
private:
    void *ptr_;
    // 禁用拷贝构造和赋值运算，保证独占所有权
    UniquePtr(const UniquePtr&) = delete;
    UniquePtr& operator=(const UniquePtr&) = delete;
};
```

**1、为什么 `std::unique_ptr` 不支持拷贝？**

因为 `std::unique_ptr` 设计为独占资源，拷贝会导致多个指针管理同一份内存资源，析构时重复释放，所以从语法上禁用拷贝，仅支持移动转移所有权。

**2、标准库 `std::unique_ptr` 还实现了自定义删除器，用于管理除内存外的其他资源。**

```cpp
int main() {
    std::unique_ptr<FILE, void(*)(FILE*)> fp(
        fopen("test.txt", "w"),
        [](FILE *p) {
            if (p) fclose(p);
        }
	);
}
```

### std::shared_ptr

`std::shared_ptr` 是共享式智能指针。

- 共享所有权：多个 `std::shared_ptr` 可以指向同一块内存，通过引用计数管理生命周期。
- 额外开销：维护堆上的引用计数，引用计数增减的原子操作。
- 支持拷贝和赋值操作：拷贝时仅增加引用计数。

**1、引用计数为什么不在对象内部？**

引用计数存储在堆内存中，保证指向同一资源的 `std::shared_ptr` 能共享同一个计数，拷贝构造时只复制该计数的指针。

**2、`std::shared_ptr` 的线程安全问题。**

- 引用计数的增减是线程安全的，增减是原子操作，多线程拷贝和销毁不会导致计数计算错误。
- 资源本身不是线程安全的，多个线程通过 `std::shared_ptr` 访问或修改资源需要手动加锁。
- `std::shared_ptr` 对象的读写不是线程安全的，多线程对 `std::shared_ptr` 对象执行 `reset()` 或赋值操作需要加锁保护。

**3、`std::shared_ptr` 的循环引用问题。**

两个或多个 `std::shared_ptr` 相互持有对方的引用，导致引用计数无法归零，最终导致内存泄露。

```cpp
class A {
public:    shared_ptr<B> b_ptr;
}
class B {
public:    shared_ptr<A> a_ptr;
}
shared_ptr<A> a(new A());
shared_ptr<B> b(new B());
a.b_ptr = b;
b.a_ptr = a;
```

函数结束时，栈上变量 `a` 进行析构：

1. 析构 `shared_ptr<A>` 的对象 `a` 时，引用计数 - 1 后的计数从 2 变为 1。
2. 因为此时的引用计数没有减到 0，所以不会调用 `class B` 的析构函数将成员 `b_ptr` 的引用计数减 1。
3. `shared_ptr<B>` 的对象 `b` 引用计数为 2，析构 `b` 时只能将引用计数减为 1，因此也不会调用 `class A` 的析构函数将成员 `a_ptr` 的引用计数减为 0。
4. 所以对象 `a` 和对象 `b` 在析构之后的引用计数都为 1，造成内存泄露。

 栈上的 `shared_ptr` 析构只能释放自己的引用，但循环引用还让对象内部持有一份引用，这份引用因为对象没有析构而无法释放。

### std::weak_ptr

`std::weak_ptr` 是 C++11 引入的**弱引用智能指针**，设计初衷就是解决 `std::shared_ptr` 的循环引用问题，同时不干扰资源的正常释放逻辑。

- 弱引用：指向 `std::shared_ptr` 管理的资源，不增加强引用计数。
- 无资源权限：析构时不会释放资源，仅影响自身的弱引用计数。
- 需要通过 `lock()` 转换为 `shared_ptr` 才能使用。
- 通过 `expired()` 检测资源是否已经被释放，释放后 `lock()` 返回空。
- 共享计数块：与 `shared_ptr` 共用一个计数块存储弱引用计数。

```bash
计数块结构（简化版）：
┌─────────────────────────┐
│ 强引用计数（use_count）    │ → 指向资源的 shared_ptr 数量（决定资源是否释放）
├─────────────────────────┤
│ 弱引用计数（weak_count）   │ → 指向资源的 weak_ptr 数量（决定计数块是否释放）
├─────────────────────────┤
│ 资源删除器（可选）          │ → shared_ptr 析构时释放资源的逻辑
└─────────────────────────┘
```

