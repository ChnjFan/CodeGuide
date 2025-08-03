# tcpdump - 命令行抓包工具

[TOC]

## 命令说明

`tcpdump` 是一款强大的命令行抓包工具，可用于捕获和分析网络数据包。

### 指定监听接口

根据 `ifconfig` 确定要监听的网络接口。

```bash
sudo tcpdump -i en0
```

输出内容包括：时间戳、源 IP、目的 IP、协议（TCP/UDP/ICMP 等）、端口、数据包长度、 Flags（如 TCP 的 SYN/ACK 等）。

### 保存抓包结果

将捕获的数据包保存到 `.pcap` 文件（可后续用 Wireshark 分析）：

```bash
sudo tcpdump -i en0 -w capture.pcap
```

分析已保存的 `.pcap` 文件：

```bash
tcpdump -r capture.pcap
```

### 添加过滤规则

按协议过滤：

```bash
sudo tcpdump -i en0 tcp   # 只抓 TCP 协议
sudo tcpdump -i en0 udp   # 只抓 UDP 协议
sudo tcpdump -i en0 icmp  # 只抓 ICMP 协议（如 ping）
```

按端口过滤：

```bash
sudo tcpdump -i en0 port 80      # 只抓 80 端口（HTTP）
sudo tcpdump -i en0 port 443     # 只抓 443 端口（HTTPS）
sudo tcpdump -i en0 portrange 1-100  # 抓 1-100 范围内的端口
```

按 IP 地址过滤：

```bash
sudo tcpdump -i en0 host 192.168.1.100  # 只抓与该 IP 相关的包
sudo tcpdump -i en0 src 192.168.1.100   # 只抓源 IP 为该地址的包
sudo tcpdump -i en0 dst 192.168.1.100   # 只抓目的 IP 为该地址的包
```

使用 and、or、not 组合过滤条件：

```bash
# 抓 TCP 协议且目的端口为 80 的包
sudo tcpdump -i en0 tcp and dst port 80

# 抓来自 192.168.1.0/24 网段且不是 SSH（22端口）的包
sudo tcpdump -i en0 src net 192.168.1.0/24 and not port 22
```

## TCP 协议抓包

1. 创建 echo TCP 服务器

使用 `nc`（netcat）创建一个简单的 echo 服务器，它会将收到的内容原样返回：

```bash
# 在第一个终端窗口执行，创建 TCP 服务器，监听 12345 端口
nc -l 12345 -k
```

2. 使用 tcpdump 开始抓包

过滤端口 12345 的 TCP 流量：

```bash
# 先确定你的网络接口（如 lo0 是环回地址）
sudo tcpdump -i lo0 -n port 12345 and tcp
```

3. 创建客户端连接

```bash
# 连接到本地的 12345 端口
nc localhost 12345
```

客户端输入任意内容完成通信后执行 ctrlc 断开连接。

### 建链报文

1. **TCP 建链第一次握手，客户端向服务端发送 SYN 报文**：

```bash
17:13:01.270689 IP localhost.52882 > localhost.italk: Flags [S], seq 1867808426, win 65535, options [mss 16344,nop,wscale 6,nop,nop,TS val 297241296 ecr 0,sackOK,eol], length 0
```

基础信息：

- 时间戳：数据包捕获的时间，精确到微秒。
- 协议版本：IPv6 数据包。
- 源地址：本机地址，端口 52881
- 目的地址：本机地址，SIP协议端口

TCP 协议内容：

- Flags[S]：标识携带 SYN 标志位，发起连接请求。
- seq：客户端生成初始序列号，SYN 报文无数据内容，该序列号代表即将发送第一个数据字节的序号
- win：窗口大小，最大65535，用于流量控制，告诉服务端客户端只能接收这么多字节数据。
- options：选项字段。
  - mss：最大报文段长度，与链路层 MTU（最大传输单元）相关。
  - nop，用于对齐选项字段。
  - wscale：窗口扩大因子，通过该选项将窗口扩大到 65535 × 2^6。
  - TS val 297241296 ecr 0：客户端时间戳，用于计算往返时间 RTT，ecr 0 表示首次发送无确认值。
  - sack0K：支持 SACK（选择性确认），告知服务器客户端可以接收选择性确认报文，用于高效重传。
  - eol：选择列表结束标记。
- length 0：数据包没有数据。

2. **服务端收到 SYN 报文后向客户端发送 SYN+ACK 报文**：

```bash
17:13:01.270734 IP localhost.italk > localhost.52882: Flags [S.], seq 2622783217, ack 1867808427, win 65535, options [mss 16344,nop,wscale 6,nop,nop,TS val 1299456513 ecr 297241296,sackOK,eol], length 0
```

这里服务端回复了一个 IPv4 的数据包。

TCP 协议内容：

- Flags[S.]：表示数据包携带 **SYN（同步）和 ACK（确认）标志位**，其中 `.` 代表 ACK 标志有效。
- seq：这里的序列号是服务端生成的初始序列号。
- ack：客户端 SYN 报文序列号 + 1。
- val 1299456513 ecr 297241296：这里的 ecr 297241296 是客户端发送的时间戳值，用于客户端校准 RTT。

3. **客户端收到服务端应答后，返回应答报文结束建链**：

```bash
17:13:01.270746 IP localhost.52882 > localhost.italk: Flags [.], ack 1, win 6380, options [nop,nop,TS val 297241296 ecr 1299456513], length 0
```

TCP 协议内容：

- Flags[.]：仅包含 ACK 标记。
- ack 1：标识客户端已收到服务器初始序列号 SYN 报文，显示 1 是抓包工具简化为相对序列号显示，表示服务器初始序列号偏移量。

TCP 连接完全建立，可以看到双方都确认了对方的初始序列号和通信参数（窗口大小、时间戳等），接下来可以进行数据传输。

### 数据传输

客户端向服务端发送数据：

```bash
17:13:10.437435 IP localhost.52882 > localhost.italk: Flags [P.], seq 1:7, ack 1, win 6380, options [nop,nop,TS val 297250463 ecr 1299456513], length 6
```

发送数据的标记为设置为 [P.] 表示推送标志，数据应立即提交给应用层，用于实时性要求高的数据。`.` 代表应答服务端的报文。

- seq 1:7：序列号范围为 1 到 7，表示本数据包携带数据从相对序列号 1 开始的 6 个字节。
- ack 1：客户端已收到服务端发送的序列号 1 的数据。

服务端收到客户端数据后应该回复一个应答报文：

```bash
17:13:10.437499 IP localhost.italk > localhost.52882: Flags [.], ack 7, win 6380, options [nop,nop,TS val 1299465680 ecr 297250463], length 0
```

这两条报文构成了一次完整的 “数据发送 - 确认” 交互：

1. 客户端通过带`P`标志的报文发送 6 字节数据，要求服务器立即处理（如 SIP 协议的短消息或请求片段）。
2. 服务器收到数据后，立即返回 ACK 报文，确认已完整接收（`ack 7`准确对应客户端发送的 6 字节数据范围）。

这一过程体现了 TCP 的**可靠传输机制**：发送方必须收到接收方的确认后，才认为数据已成功交付；若超时未收到确认，发送方会重传数据。

### 断链报文

客户端主动断链：

1. **客户端向服务端发送 FIN 报文通知断链**：

```bash
17:13:18.515120 IP localhost.52882 > localhost.italk: Flags [F.], seq 7, ack 1, win 6380, options [nop,nop,TS val 297258540 ecr 1299465680], length 0
```

TCP 标志位 [F.] 表示 FIN + ACK，此时客户端已经完成数据发送，请求关闭自己的发送方向。

2. **服务端收到客户端 FIN 报文后回复应答**：

```bash
17:13:18.515175 IP localhost.italk > localhost.52882: Flags [.], ack 8, win 6380, options [nop,nop,TS val 1299473757 ecr 297258540], length 0
```

FIN 报文的确认只包含 `.` 标记位，此时客户端状态进入“FIN-WAIT-2”状态，等待服务端发送自己的 FIN 报文，服务端进入“CLOSE-WAIT”状态，准备关闭自己的发送方向。

3. **服务端主动发送 FIN 报文**：

```bash
17:13:18.515200 IP localhost.italk > localhost.52882: Flags [F.], seq 1, ack 8, win 6380, options [nop,nop,TS val 1299473757 ecr 297258540], length 0
```

4. **客户端对服务端 FIN 报文最终应答**：

 ```bash
 17:13:18.515253 IP localhost.52882 > localhost.italk: Flags [.], ack 2, win 6380, options [nop,nop,TS val 297258540 ecr 1299473757], length 0
 ```

服务端收到确认报文后进入 “LAST-ACK” 状态，客户端进入 “TIME-WAIT” 状态（等待一段时间确保服务器收到确认），最终双方关闭连接。
