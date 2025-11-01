# HTTP 协议

HTTP（Hypertext Transfer Protocol，超文本传输协议）是**用于在客户端和服务器之间传输超文本（如 HTML、图片、视频等）的应用层协议**，是 Web 的应用层协议。通过客户端向服务端建立 TCP 连接传输数据。

[TOC]

## 报文格式

HTTP 请求报文由**请求行、请求头、空行、请求体**四部分组成：

```http
GET /index.html HTTP/1.1  // 请求行
Host: www.example.com     // 请求头（键值对）
User-Agent: Chrome/90.0.0.0
Accept: text/html,application/xhtml+xml

username=test&password=123  // 请求体（可选，如POST数据）
```

- 请求行：包括操作类型（GET、POST、HEAD、PUT 和 DELETE），URL 字段（目标资源路径）和 HTTP 版本号（HTTP/1.1、HTTP/2）。
- 请求头：使用键值对描述请求的附加信息。
  - Host：服务器域名或 IP 地址。
  - User-Agent：客户端身份信息。
  - Content-Type：请求体的数据格式。
  - Cookie：客户端存储的会话信息。
  - Connection：值为 Keep-Alive 使用长连接机制。HTTP/1.1 版本默认是长连接，为了兼容旧版本设置该字段。
- 请求体：向服务器提交数据时存在。

HTTP 响应报文由**状态行、响应头、空行、响应体**四部分组成：

```http
HTTP/1.1 200 OK  // 状态行
Date: Mon, 31 Oct 2025 12:00:00 GMT  // 响应头
Content-Type: text/html; charset=utf-8
Content-Length: 1024

<html><body>Hello World</body></html>  // 响应体（资源内容）
```

- 状态行：协议版本字段、状态码和状态描述信息
- 响应头：使用键值对描述响应附加信息。
  - Content-Type：响应体的数据格式。
  - Content-Length：响应体的字节长度。
  - Set-Cookie：服务端向客户端设置 Cookie。
  - Cache-Control：缓存控制策略。
- 响应体：服务器返回资源内容。

### 状态码

HTTP 状态码是服务器对客户端请求的结果，共五大类。

1. **1xx（信息性状态码）：请求已接收，需继续处理**
2. **2xx（成功状态码）：请求已成功处理**
   - **200 OK**：请求完全成功，响应体包含请求的资源（如 GET 网页、API 查询数据）。
   - **204 No Content**：请求成功，但响应体无内容（多用于 DELETE 请求、只需要确认状态的操作）。
   - **206 Partial Content**：部分内容请求成功（客户端通过 Range 头请求文件片段，如视频断点续传、大文件分块下载）。

3. **3xx（重定向状态码）：需客户端进一步操作**

   重定向状态码表示客户端请求的资源发生变动，需要客户端使用新的 URL 重新获取资源。

   - **301 Moved Permanently**：资源永久迁移到新 URL，后续请求需使用新地址（浏览器会缓存该重定向，下次直接访问新 URL）。
   - **302 Found**：资源临时迁移到新 URL，下次请求仍可使用原地址（不缓存，常用于临时跳转，如登录后跳回原页面）。
   - **304 Not Modified**：资源未修改（客户端携带缓存验证信息，如 If-Modified-Since，服务器告知可直接使用本地缓存，无需重新传输资源）。

4. **4xx（客户端错误状态码）：请求有误，服务器无法处理**
   - **400 Bad Request**：请求参数错误、格式非法（如 JSON 语法错误、表单字段缺失，服务器无法解析）。
   - **403 Forbidden**：服务器拒绝请求（已认证，但无访问权限，如普通用户访问管理员接口、IP 被拉黑）。
   - **404 Not Found**：请求的资源不存在（URL 错误、资源已删除，是最常见的客户端错误）。

5. **5xx（服务器错误状态码）：服务器处理请求时出错**
   - **500 Internal Server Error**：服务器通用错误（最常见的服务器错误，如代码 Bug、数据库连接失败，具体原因需看服务器日志）。
   - **501 Not Implemented**：服务器未实现该请求方法（如客户端用了服务器不支持的扩展方法）。
   - **502 Bad Gateway**：网关错误（服务器作为网关 / 代理，从上游服务器接收了无效响应，如反向代理后后端服务宕机）。
   - **503 Service Unavailable**：服务暂时不可用（服务器过载、维护中，通常是临时状态，响应头可能包含 Retry-After 告知重试时间）。

### 请求类型

