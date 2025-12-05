# 线程管理

线程是操作系统（CPU）调度的最小执行单元，也是进程内的执行单元，进程是资源分配的最小单位。

一个进程可以包含多个线程，所有线程共享进程的内存空间（代码段、数据段和堆），拥有独立的栈空间和寄存器上下文。

[TOC]

## 创建线程

`std::thread` 是 C++11 标准库 `<thread>` 头文件中提供的核心类，用于创建和管理操作系统级别的线程。

它封装了底层平台（`Linux/pthread`、`Windows/CreateThread`）的线程接口，提供跨平台的线程操作能力。

```cpp
#include <thread>
std::thread t(callable, arg1, arg2, ...);
```

1. **线程的本质**：`std::thread` 创建的是**内核线程**（操作系统调度的最小单元），每个 `std::thread` 对象对应一个内核线程（TID）。
2. **可调用对象**：线程执行的逻辑需通过 “可调用对象” 传入，包括：函数指针、lambda 表达式、函数对象（仿函数）、`std::bind` 绑定的函数、类成员函数。
3. **线程的生命周期**：`std::thread` 对象创建后，线程立即启动执行；必须通过 `join()` 或 `detach()` 管理线程资源，否则析构时会调用 `std::terminate()` 终止程序。

### 参数传递方式

线程执行函数和参数传递：

```cpp
void printMsg(const string& msg, int num) {
    cout << "Thread ID: " << this_thread::get_id() << ", Msg: " << msg << ", Num: " << num << endl;
}

int main() {
    // 创建线程：传入函数名 + 函数参数
    thread t1(printMsg, "Hello World", 100);
    // 等待线程执行完毕（必须调用 join()/detach()）
    t1.join();
    return 0;
}
```

Lambda 表达式无需定义函数，适合较短逻辑：

```cpp
int main() {
    int x = 10;
    string str = "Lambda Thread";
    
    // 创建线程：Lambda 捕获外部变量 + 执行逻辑
    thread t2([x, &str]() {  // 按值捕获x，按引用捕获str
        str += " (modified)";
        cout << "Lambda Thread ID: " << this_thread::get_id() 
             << ", x: " << x << ", str: " << str << endl;
    });
    
    t2.join();
    cout << "Main Thread str: " << str << endl;  // str已被修改（引用捕获）
    return 0;
}
```

> Lambda 捕获引用时，要确保被引用的变量生命周期比线程长，否则会访问悬空引用。实现中最好使用 `join()` 而不是 `detach()`。

重载 `operator()` 的函数对象：

```cpp
// 定义函数对象（重载 operator()）
struct Task {
    void operator()(int count) {
        for (int i = 0; i < count; ++i) {
            cout << "Functor Thread: " << i << endl;
        }
    }
};

int main() {
    // 方式1：传入函数对象实例
    Task task;
    thread t3(task, 3);  // 执行 task.operator()(3)
    // 方式2：直接构造临时对象（注意括号：避免解析为函数声明）
    thread t4(Task(), 2);  // 等价于 thread t4{Task(), 2};
    
    t3.join();
    t4.join();
    return 0;
}
```

类成员变量：

```cpp
class MyClass {
public:
    void memberFunc(int num) {
        cout << "Member Function Thread ID: " << this_thread::get_id() 
             << ", Num: " << num << endl;
    }
};

int main() {
    MyClass obj;
    // 创建线程：&类名::成员函数, 实例指针, 成员函数参数
    thread t5(&MyClass::memberFunc, &obj, 5);  // &obj 是this指针
    
    t5.join();
    return 0;
}
```

`std::bind` 绑定函数：

```cpp
#include <functional>  // std::bind 头文件

void add(int a, int b, int& result) {
    result = a + b;
}

int main() {
    int res = 0;
    // 绑定函数参数：绑定a=10, b=20，result为引用
    auto bindFunc = bind(add, 10, 20, ref(res));  // ref() 传递引用
    
    thread t6(bindFunc);
    t6.join();
    
    cout << "Result: " << res << endl;  // 输出30
    return 0;
}
```

## 线程管理

线程从创建到销毁的完整流程，需要确保按需启动，有序退出，避免**僵尸线程**（已结束但资源未释放）和**线程泄露**（未正常销毁线程导致资源耗尽）。

线程的状态包括：就绪（等待 CPU 调度）、运行（占用 CPU 执行任务）、阻塞（因等待资源 / 事件暂停，如等待 IO、锁）、销毁（任务完成或异常终止）。

### 线程等待

`join()` 是 `std::thread` 最核心的成员函数之一：

- 作用：主线程**阻塞**，等待当前线程执行完毕后，回收线程资源（栈、内核线程描述符）；
- 特性：
  1. 一个线程只能调用一次 `join()`，调用后 `joinable()` 返回 `false`；
  2. 若线程已执行完毕，`join()` 会立即返回，不会阻塞；
  3. 必须在 `std::thread` 对象析构前调用 `join()`（否则析构函数会终止程序）。

```cpp
void task(int id) {
    this_thread::sleep_for(chrono::milliseconds(100));  // 模拟耗时
    cout << "Task " << id << " done" << endl;
}
int main() {
    thread t1(task, 1);
    thread t2(task, 2); 
    // 等待所有线程完成（主线程阻塞，直到t1、t2都执行完）
    t1.join();
    t2.join();
    cout << "All tasks done" << endl;
    return 0;
}
```

### 线程分离

`detach()` 将线程与 `std::thread` 对象分离，线程成为**后台线程**（守护线程）：

- 作用：线程的资源由**操作系统自动回收**（无需主线程等待）；
- 特性：
  1. 分离后的线程无法再调用 `join()`（`joinable()` 返回 `false`）；
  2. 分离的线程不能访问主线程的局部变量（局部变量可能先销毁，导致悬空引用 / 指针）；
  3. 进程退出时，所有分离的线程会被强制终止（无论是否执行完毕）。

```cpp
void backgroundTask() {
    for (int i = 0; i < 5; ++i) {
        this_thread::sleep_for(chrono::seconds(1));
        cout << "Background thread running: " << i << endl;
    }
    cout << "Background thread done" << endl;
}
int main() {
    thread t(backgroundTask);
    t.detach();  // 分离线程，主线程无需等待
    
    cout << "Main thread exit after 3 seconds..." << endl;
    this_thread::sleep_for(chrono::seconds(3));
    return 0;  // 主线程退出，后台线程被强制终止（仅执行到i=2）
}
```

