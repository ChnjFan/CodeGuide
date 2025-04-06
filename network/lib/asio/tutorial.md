# Boost.Asio 示例

> 教程示例代码使用 Asio 库实现简单的异步定时器和客户端服务的通信。

## 定时器

在进入复杂的网络编程前，首先了解简单的定时器。

### 同步定时器

示例代码演示如何执行阻塞等待计时器：

```cpp
#include <iostream>
#include <asio.hpp>     // 包含头文件

int main()
{
    asio::io_context io;    // 所有 asio 程序都需要定义 I/O 执行上下文对象，提供 I/O 功能的访问
    asio::steady_timer t(io, asio::chrono::seconds(5)); // 创建定时器
    t.wait();   // 定时器 expired 状态会立即返回
    std::cout << "Hello, world!" << std::endl;
    return 0;
}
```

### 异步定时器

异步定时器不会像同步定时器执行阻塞等待，通过提供一个 completion token，当操作完成后结果会传递给指定的完成处理函数。

```cpp
void print(const asio::error_code& e) {
    std::cout << "Hello, world!" << std::endl;
}

int main()
{
    asio::io_context io;
    asio::steady_timer t(io, asio::chrono::seconds(5));
    t.async_wait(&print);
    std::cout << "async wait" << std::endl;
    io.run();   // 启动事件循环，持续到所有异步操作完成
    return 0;
}
```

**绑定参数到完成处理函数**

实现重复计时器，需要在定时器处理函数中获取到 timer 对象。

默认情况 `async_wait` 的完成处理器只接受一个表示错误状态的参数，其他参数需要使用 `std::bind` 绑定到完成处理器上。

```cpp
#include <functional>
#include <iostream>
#include <boost/asio.hpp>

void print(const boost::system::error_code& /*e*/,
           boost::asio::steady_timer* t, int* count) {//增加参数
    if (*count < 5) {
        std::cout << *count << std::endl;
        ++(*count);
        // 重新计算定时器的到期时间：当前到期时间+定时器时间
        t->expires_at(t->expiry() + boost::asio::chrono::seconds(1));
        t->async_wait(std::bind(print, boost::asio::placeholders::error, t, count));
    }
}

int main() {
    boost::asio::io_context io;
    int count = 0;
    boost::asio::steady_timer t(io, boost::asio::chrono::seconds(1));
    t.async_wait(std::bind(print, boost::asio::placeholders::error, &t, &count));
    io.run();
    std::cout << "Final count is " << count << std::endl;
    return 0;
}
```

**使用成员函数作为完成处理函数**

将定时器封装到类中，构造对象时创建定时器并启动定时器。

```cpp
#include <functional>
#include <iostream>
#include <boost/asio.hpp>

class printer {
public:
    printer(boost::asio::io_context& io)
            : timer_(io, boost::asio::chrono::seconds(1)),
              count_(0) {
        timer_.async_wait(std::bind(&printer::print, this)); // 绑定成员函数和对象
    }

    ~printer() {
        std::cout << "Final count is " << count_ << std::endl;
    }

    void print() {
        if (count_ < 5) {
            std::cout << count_ << std::endl;
            ++count_;
            timer_.expires_at(timer_.expiry() + boost::asio::chrono::seconds(1));
            timer_.async_wait(std::bind(&printer::print, this));
        }
    }

private:
    boost::asio::steady_timer timer_;
    int count_;
};

int main() {
    boost::asio::io_context io;
    printer p(io);
    io.run();
    return 0;
}
```

