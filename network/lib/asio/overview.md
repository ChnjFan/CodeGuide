# Boost.Asio 概述

> Boost.Asio 是一个跨平台的 C++ 库，主要用于网络编程和其他底层输入/输出（I/O）操作。它提供了一致的异步 I/O 模型，使开发者能够编写高效、可扩展的网络应用程序。

## 介绍

**主要特点：**

- **异步 I/O 支持**：Boost.Asio 允许开发者以异步方式处理数据，这意味着在等待某些操作完成时，程序可以继续执行其他任务，提高了应用程序的响应性和性能。
- **跨平台兼容性**：该库支持多种操作系统，包括 Windows、Linux 和 macOS，使其适用于各种平台的开发。
- **丰富的功能**：Boost.Asio 提供了对 TCP、UDP、ICMP 等协议的支持，并支持同步和异步操作，适用于构建各种类型的网络应用。

**基本组成：**

- **I/O 服务（io_service）**：Boost.Asio 的核心，负责管理所有异步操作的执行。
- **I/O 对象**：如套接字（socket）和定时器（timer），用于执行具体的 I/O 操作。

### 编译安装

1. 在 [boost](https://archives.boost.io/release/) 官方网站下载对应版本的 Boost 库源代码。
2. 解压后进入对应目录后，执行 `bootstrap.sh` 和 `b2` 并指定安装目录。例如安装到 /home 目录。

```shell
./bootstrap.sh --prefix=/home/
./b2 install --prefix=/home/
```

配置项目的 CMakeLists.txt 文件中，添加：

```cmake
  set(BOOST_ROOT "/home")
  find_package(Boost REQUIRED COMPONENTS system)
  include_directories(${Boost_INCLUDE_DIRS})
  target_link_libraries(your_project_name ${Boost_LIBRARIES})
```

### 基础结构

Boost.Asio 可以在 I/O 对象（如socket）上执行同步或异步操作。

程序至少有一个 **I/O 执行上下文**，表示程序连接到操作系统的 I/O 服务。

```cpp
boost::asio::io_context io_context;
```

执行 I/O 操作还需要一个 I/O 对象，例如 TCP 套接字：

```cpp
boost::asio::ip::tcp::socket socket(io_context);
```

**同步操作**

执行同步连接时，以下事件将顺序执行：

![sync_op](./sync_op.png)

1. 程序通过调用 I/O 操作启动 connect 操作：

```cpp
socket.connect(server_endpoint);
```

2.  **I/O 对象**将请求转发到 **I/O 执行上下文**。
3. **I/O 执行上下文**调用系统调用以执行连接操作。
4. 系统调用的结果添加到 **I/O 执行上下文**中。

5. **I/O 执行上下文**将系统调用的错误结果转换为对象类型 `boost::system::error_code``error_code``false`，然后将结果转发到 I/O 对象。
6. 如果执行失败， **I/O 对象**将抛出异常。

如果入参添加获取错误码，则不会抛出异常：

```cpp
boost::system::error_code ec;
socket.connect(server_endpoint, ec);
```

同步操作示例：

```cpp
#include <boost/asio.hpp>
#include <iostream>

int main() {
    try {
        boost::asio::io_context io_context;
        boost::asio::ip::tcp::socket socket(io_context);
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), 12345);
        boost::system::error_code ec;
        socket.connect(endpoint, ec);
        if (ec) {
            std::cout << "连接失败: " << ec.message() << std::endl;
            return 1;
        }
        std::cout << "连接成功!" << std::endl;
        socket.close();     // 关闭套接字
    } catch (const std::exception& e) {
        std::cout << "异常: " << e.what() << std::endl;
    }

    return 0;
}
```

**异步操作**

异步操作是与同步操作不同的事件序列：

![async_op1](./async_op1.png)

1. 通过调用 **I/O 对象**启动 connect 的异步操作：

```cpp
socket.async_connect(server_endpoint, your_completion_handler);
```

其中 `your_completion_handler` 的函数原型为：

```cpp
void your_completion_handler(const boost::system::error_code& ec);
```

2. **I/O 对象**将请求转发到 **I/O 执行上下文**。

3. **I/O 执行上下文**调用系统调用以执行连接操作。此时与同步操作阻塞不同，异步操作会立即返回，允许其他擦操作继续执行。

   ![async_op2](./async_op2.png)

4. 操作系统在后台尝试建立连接，连接完成后将结果放入队列中，等待 **I/O 执行上下文**来检索。
5.  **I/O 执行上下文**通过调用 `io_context.run()` 检索操作结果，其中 `io_context.run()` 会一直阻塞直到所有异步操作完成。
6. 在 `io_context.run()` 的调用过程中，**I/O 执行上下文**会从队列中取出操作结果，将其转换为 `error_code` 类型，并传递给预先定义的 `your_completion_handler` 处理程序，以便应用程序处理。

需要注意的是，`io_context.run()` 会阻塞当前线程，直到所有异步操作完成。如果希望在多个线程中并行处理异步操作，可以创建多个 `io_context` 实例，并在不同的线程中运行它们。

异步连接示例：

```cpp
#include <boost/asio.hpp>
#include <iostream>

void connect_handler(const boost::system::error_code& error)
{
    if (!error) std::cout << "连接成功！" << std::endl;
    else std::cout << "连接失败: " << error.message() << std::endl;
}

int main()
{
    try {
        boost::asio::io_context io_context;					// 创建 I/O 执行上下文
        boost::asio::ip::tcp::socket socket(io_context);	// 创建 I/O 对象
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), 12345);
        socket.async_connect(endpoint, connect_handler);
        io_context.run(); // 运行 io_context 阻塞到异步操作完成
    }
    catch (const std::exception& e) {
        std::cerr << "异常: " << e.what() << std::endl;
    }
    return 0;
}
```

## 异步模型

Boost.Asio 库的核心是基于异步模型的，这种模型将异步操作作为构建异步组合的基本单元，同时将其与组合机制解耦。这种设计使得开发者可以灵活地选择不同的异步处理方式，如回调函数、future（包括饿汉式和懒汉式）、fibers、协程等，以满足不同场景的需求。

**异步模型的优势：**

- **高效利用系统资源：** 通过异步操作，程序可以在等待 I/O 操作完成期间执行其他任务，避免了线程阻塞，提高了系统的并发性能。
- **灵活的编程方式：** 开发者可以根据具体需求选择最适合的异步处理方式，如回调函数、future、协程等，提供了多样的编程范式。

这种异步模型的设计，使得 Boost.Asio 成为一个强大的工具，适用于需要高性能和高并发的网络编程场景。

## 异步操作

异步操作是 Boost.Asio 异步模型中的基本组成单元。异步操作表示在后台启动并执行的工作，而发起这些操作的用户代码可以继续执行其他任务。

**异步操作的生命周期：**

1. **发起函数（Initiating Function）：** 由用户调用以启动异步操作的函数。
2. **完成处理器（Completion Handler）：** 用户提供的、只能调用一次的函数对象，在异步操作完成时被调用，传递操作结果。

**同步与异步操作的语义对比：**

- **返回类型推导：** 同步操作的返回类型由函数模板及其参数决定；异步操作的完成处理器的参数类型和顺序由发起函数及其参数决定。
- **临时资源释放：** 同步操作在返回前释放临时资源；异步操作在调用完成处理器前释放临时资源。这确保了完成处理器在不与其他操作重叠使用资源的情况下启动。

## 异步代理

在 Boost.Asio 中，**异步代理（Asynchronous Agents）** 是由一系列异步操作组成的顺序结构。每个异步操作都在异步代理中执行，异步代理本身可以与其他代理并发运行。这种设计使得异步代理类似于线程在同步操作中的作用，但它们是纯粹的概念性构造，用于描述程序中异步操作的上下文和组合方式。

**异步代理的关键特性：**

- **异步操作的组合：** 异步代理将多个异步操作按顺序组合起来，每个操作完成后执行相应的完成处理程序。
- **并发执行：** 异步代理可以与其他代理并发执行，提高程序的并发性和性能。
- **概念性构造：** 异步代理用于描述异步操作的组织和调度方式，并不对应于库中的具体类型或类。

**示意图：**

![async_agent_chain](D:\项目文件\CodeGuide\network\lib\asio\async_agent_chain.png)

可以将异步代理视为一个循环结构，不断等待异步操作完成，并执行相应的处理程序。

通过引入异步代理的概念，Boost.Asio 提供了一种高效、灵活的方式来组织和管理异步操作，使得开发者能够根据具体需求选择最适合的异步处理模型。

## 核心概念和功能

Boost.Asio 是一个跨平台的 C++ 库，旨在提供一致的异步 I/O 操作支持。其异步模型基于 **Proactor 设计模式**，该模式通过将异步操作的执行与完成处理程序的调用解耦，简化了并发编程。

### Proactor 设计模式

**Proactor 设计模式在 Boost.Asio 中的实现：**

1. **异步操作（Asynchronous Operation）：** 例如，在套接字上执行异步读写操作。
2. **异步操作处理器（Asynchronous Operation Processor）：** 执行异步操作，并在操作完成时将事件放入完成事件队列。
3. **完成事件队列（Completion Event Queue）：** 缓冲完成事件，直到它们被异步事件多路分解器取出。
4. **完成处理程序（Completion Handler）：** 处理异步操作的结果，通常为函数对象。
5. **异步事件多路分解器（Asynchronous Event Demultiplexer）：** 阻塞等待完成事件队列中的事件，并将完成的事件返回给调用者。
6. **Proactor：** 调用异步事件多路分解器以取出事件，并调度与事件关联的完成处理程序。
7. **发起者（Initiator）：** 应用程序特定代码，启动异步操作。

在许多平台上，Boost.Asio 将 Proactor 设计模式实现为 Reactor 模式，例如使用 `select`、`epoll` 或 `kqueue`。这种实现方式将各个组件对应如下：

- **异步操作处理器（Asynchronous Operation Processor）：** 使用 `select`、`epoll` 或 `kqueue` 等 Reactor 实现。当 Reactor 指示资源准备好执行操作时，处理器执行异步操作，并将关联的完成处理程序放入完成事件队列。
- **完成事件队列（Completion Event Queue）：** 一个链表，存储完成处理程序（即函数对象）。
- **异步事件多路分解器（Asynchronous Event Demultiplexer）：** 通过等待事件或条件变量，直到完成事件队列中有完成处理程序可用。

这种设计使 Boost.Asio 能够在不同平台上提供一致的异步 I/O 操作支持，同时利用各平台的特性来实现高效的事件处理。

### 线程

在 Boost.Asio 中，`io_context`（在较早版本中称为 `io_service`） 是管理异步操作的核心对象。为了充分利用多核处理器的性能，可以在多个线程中调用 `io_context::run()`，从而实现多线程并发处理。这种方法通过创建一个线程池，使多个线程共同处理异步操作，提高了程序的响应性和吞吐量。

**线程安全**

Boost.Asio 保证，多个线程可以安全地并发调用 `io_context::run()`，而不需要额外的同步机制。这意味着，可以在多个线程中共享同一个 `io_context` 实例，库会负责在这些线程之间分配工作。

- **线程池中的线程是平等的：** `io_context` 会在所有调用了 `io_context::run()` 的线程之间随机分配工作负载。

- **避免在处理程序中执行阻塞操作：** 在异步操作的完成处理程序中执行阻塞操作可能会阻塞整个线程池，影响性能。
- **使用 `io_context::strand` 保证处理程序的顺序执行：** 如果多个线程可能同时调用同一个处理程序，使用 `io_context::strand` 可以确保这些调用是顺序执行的，避免竞态条件。

**Strands**

**Strands** 是一种用于在多线程环境中顺序执行异步事件处理程序的机制，旨在避免显式的锁操作（如使用互斥锁）。通过使用 Strands，可以确保在多线程程序中对共享资源的访问是线程安全的，同时避免因显式锁定带来的性能开销。

- **隐式 Strand：**

  - 当在单个线程中调用 `io_context::run()` 时，所有事件处理程序都在该线程中顺序执行，这相当于使用了一个隐式的 Strand。

  - 在某些协议（如 HTTP）中，如果每个连接都有一个独立的异步操作链且没有并发执行的需求，这也可以视为隐式 Strand。


- **显式 Strand：**

  - 使用 `strand<>` 或 `io_context::strand` 创建的显式 Strand，需要将所有事件处理程序绑定到该 Strand 上。

  - 可以使用 `boost::asio::bind_executor()` 将事件处理程序与 Strand 关联。例如：


```cpp
my_socket.async_read_some(my_buffer,
    boost::asio::bind_executor(my_strand,
      [](boost::system::error_code ec, std::size_t length)
      {
        // 处理读取操作
      }));
```

### Buffers

缓冲区（Buffers）是用于存储通过 I/O 操作传输数据的内存区域。为了支持 scatter-gather 操作，即一次读取数据到多个缓冲区或一次写入多个缓冲区，Boost.Asio 提供了对缓冲区的抽象表示。

**缓冲区类型：**

- **`typedef std::pair<void*, std::size_t> mutable_buffer;`：** 表示可修改的缓冲区，通常用于接收数据。

- **`typedef std::pair<const void*, std::size_t> const_buffer;`：** 表示不可修改的缓冲区，通常用于发送数据。

使用示例：

```cpp
int main() {
    try {
        boost::asio::io_context io_context;
        boost::asio::ip::tcp::socket socket(io_context);
        boost::asio::ip::tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve("example.com", "http");
        boost::asio::connect(socket, endpoints);

        // 创建一个可修改的缓冲区用于接收数据
        std::array<char, 128> recv_buffer;
        boost::asio::mutable_buffer mutable_buf(recv_buffer.data(), recv_buffer.size());
        // 创建一个不可修改的缓冲区用于发送数据
        const std::string msg = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
        boost::asio::const_buffer const_buf(msg.data(), msg.size());

        boost::asio::write(socket, boost::asio::buffer(const_buf));
        size_t len = boost::asio::read(socket, boost::asio::buffer(mutable_buf));
        std::cout.write(recv_buffer.data(), len);
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}
```

**按行读取**

在 Boost.Asio 中，处理基于行的传输操作通常涉及使用 `boost::asio::read_until` 或 `boost::asio::async_read_until` 函数。这些函数允许程序在读取数据时，直到遇到指定的分隔符（如换行符）为止，从而方便地处理基于行的协议，如 HTTP、SMTP 或 FTP 等。

假设我们需要从一个 TCP 套接字中读取一行以换行符 `'\n'` 结束的数据，并将其存储在 `std::string` 中。以下是实现的示例代码：

```cpp
boost::asio::streambuf response;
boost::asio::read_until(socket, response, '\n');
```

### 异步调试

Boost.Asio 的处理器跟踪功能主要用于调试异步操作，通过记录处理器（即回调函数）的源代码位置，帮助开发者追踪异步操作的执行流程。

**启用处理器跟踪**

要启用处理器跟踪功能，需要在包含任何 Boost.Asio 头文件之前，定义宏 `BOOST_ASIO_HANDLER_TRACKING`。可以在代码中添加以下行来实现：

```cpp
#define BOOST_ASIO_HANDLER_TRACKING 1
```

这将使 Boost.Asio 在生成处理器时包含源代码位置的信息。

**查看源代码位置信息**

启用处理器跟踪后，Boost.Asio 会在处理器调用时输出源代码的位置，包括文件名、行号和函数名。例如：

```shell
n^m: file_name.cpp(123): function_name
```

其中，`n` 和 `m` 表示源代码中的行号，帮助开发者定位处理器的定义和调用位置。

