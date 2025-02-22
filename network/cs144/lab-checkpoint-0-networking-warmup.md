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

* S .表示源代码目录是当前目录（minnow）。
* B build表示编译输出目录为build。

## 编写 webget 程序

阅读 socket.hh 和 file\_descriptor.hh，在 get\_URL 函数中实现一个简单的 Web 客户端，使用 HTTP 协议格式发送请求，并接收服务器的响应。

1. 打开文件并编辑代码

从 build 目录中，打开文件../apps/webget.cc，这是 webget 程序的源代码文件。 在 get\_URL 函数中实现一个简单的 Web 客户端，使用 HTTP 协议格式发送请求，并接收服务器的响应。

2. 实现HTTP请求

使用TCPSocket和Address类来建立与Web服务器的连接。

根据HTTP协议规范，构造一个GET请求，包括以下内容：

* 请求行：GET /path HTTP/1.1（将/path替换为目标网页的路径）。
* Host头：Host: hostname（将hostname替换为目标服务器的域名）。
* Connection: close头：告诉服务器在发送完响应后关闭连接。

注意：HTTP协议中，每行必须以\r\n结尾，而不是仅\n或endl。

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

* 编译程序：运行 cmake --build build。
* 测试程序：运行 ./apps/webget cs144.keithw.org /hello，观察输出是否与浏览器访问 http://cs144.keithw.org/hello 的结果一致。
* 运行自动化测试：执行cmake --build build --target check\_webget，确保程序通过所有测试用例。

```shell
➜  minnow git:(main) ✗ ./build/apps/webget cs144.keithw.org /hello
GET/helloHTTP/1.1
Host: cs144.keithw.org


HTTP/1.1 200 OK
Date: Sat, 22 Feb 2025 00:35:19 GMT
Server: Apache
Last-Modified: Thu, 13 Dec 2018 15:45:29 GMT
ETag: "e-57ce93446cb64"
Accept-Ranges: bytes
Content-Length: 14
Content-Type: text/plain

Hello, CS144!
```
