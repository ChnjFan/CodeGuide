# Lab Checkpoint 0: networking warmup

## 代码库下载编译

克隆 Minnow 代码库，并将本地代码库与 Github 仓库关联

```bash
git clone https://github.com/cs144/minnow CS144
cd CS144
git remote add github https://github.com/ChnjFan/minnow
git push github
```

编译启动代码

使用 cmake 创建一个编译目录，在 build 目录中编译代码。

```bash
cmake -S . -B build
cmake --build build
```

* `S .` 表示源代码目录是当前目录（minnow）。
* `B build` 表示编译输出目录为 build。

## 编写 webget 程序

阅读 socket.hh 和 file\_descriptor.hh，在 get\_URL 函数中实现一个简单的 Web 客户端，使用 HTTP 协议格式发送请求，并接收服务器的响应。

1. 打开文件并编辑代码

从 build 目录中，打开文件 ../apps/webget.cc，这是 webget 程序的源代码文件。 在 get\_URL 函数中实现一个简单的 Web 客户端，使用 HTTP 协议格式发送请求，并接收服务器的响应。

2. 实现 HTTP 请求

使用 TCPSocket 和 Address 类来建立与 Web 服务器的连接。

根据 HTTP 协议规范，构造一个GET请求，包括以下内容：

* 请求行：GET /path HTTP/1.1（将/path替换为目标网页的路径）。
* Host头：Host: hostname（将hostname替换为目标服务器的域名）。
* Connection: close 头告诉服务器在发送完响应后关闭连接。

注意：HTTP 协议中，每行必须以 `\r\n` 结尾，而不是仅 `\n` 或 `endl`。

```c++
void get_URL( const string& host, const string& path )
{
  TCPSocket socket;
  socket.connect(Address(host, "http"));
  socket.write("GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\n\r\n");
  cout << "GET" + path + "HTTP/1.1\r\nHost: " + host + "\r\n\r\n" << endl;
  socket.shutdown(SHUT_WR);
  while (!socket.eof()) {
    std::string content;
    socket.read(content);
    cout <<content;
  }
  socket.close();
}
```

3. 读取服务器响应

从服务器读取响应内容，直到套接字达到“EOF”（文件结束符），表示服务器已经发送完所有内容。打印从服务器接收到的所有内容。

4. 测试程序

* 编译程序：运行 `cmake --build build`。
* 测试程序：运行 `./apps/webget cs144.keithw.org /hello`，观察输出是否与浏览器访问 http://cs144.keithw.org/hello 的结果一致。
* 运行自动化测试：执行 `cmake --build build --target check\_webget`，确保程序通过所有测试用例。

```shell
➜  minnow git:(main) ✗ cmake --build build --target check\_webget
Test project /home/fan/minnow/build
    Start 1: compile with bug-checkers
1/2 Test #1: compile with bug-checkers ........   Passed    0.13 sec
    Start 2: t_webget
2/2 Test #2: t_webget .........................   Passed    1.40 sec

100% tests passed, 0 tests failed out of 2

Total Test time (real) =   1.53 sec
Built target check_webget
```

## 实现有序字节流

字节流是一个抽象的数据结构，用于模拟网络通信中的可靠字节流传输。它需要实现以下功能：

1. 字节流的写入和读取：
  - 字节流有一个“输入端”和一个“输出端”。
  - 数据可以从输入端写入，并且能够以相同的顺序从输出端读出。
  - 写入端可以结束输入，之后不能再写入数据。
  - 读取端在读到流的结尾时会到达“EOF”（文件结束）状态，之后不能再读取数据。
  
2. 流量控制（Flow Control）：
  - 字节流有一个“容量”（capacity），表示它在任何时刻愿意存储的最大字节数。
  - 写入端在写入数据时，不能超过当前可用的容量。
  - 当读取端读取数据并从流中移除数据后，写入端可以继续写入更多数据，以确保流的总存储量不超过容量限制。

3. 注意事项：
  - 提供了写入端和读取端的接口，包括：
    - 写入端：push(std::string data)、close()、is_closed()、available_capacity()、bytes_pushed()。
    - 读取端：peek()、pop(uint64_t len)、is_finished()、has_error()、bytes_buffered()、bytes_popped()。
- 理解字节流的抽象概念：字节流是一个抽象的数据结构，它模拟了网络通信中的可靠字节流传输。理解其抽象概念和行为是实现的关键。
- 流量控制的实现：流量控制是字节流的重要特性。需要确保写入端不会因为写入过多数据而导致内存溢出，同时读取端能够正确地读取数据并释放空间。
- 接口的完整性和正确性：实现时需要严格按照提供的接口定义进行，确保所有功能都能正常工作。例如，push方法需要正确处理容量限制，peek和pop方法需要正确管理缓冲区。