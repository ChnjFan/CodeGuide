# netstat - 查看网络连接状态

[TOC]

## 命令说明

`netstat` 是一个用于查看网络状态和连接信息的命令行工具。它能帮助用户监控网络连接、端口占用、路由表、网络接口统计等信息，是网络故障排查和系统监控的常用工具。

## 主要作用

- **显示网络连接**：列出当前系统与外部的 TCP/UDP 连接，包括本地地址、远程地址、连接状态（如 `ESTABLISHED` 已建立、`LISTEN` 监听等）。

```bash
fan@FandeMac-mini ~ % netstat -ant
Active Internet connections (including servers)
Proto Recv-Q Send-Q  Local Address          Foreign Address        (state)    
tcp4       0      0  *.22                   *.*                    LISTEN     
tcp6       0      0  *.22                   *.*                    LISTEN     
```

- **查看端口占用**：快速定位哪个进程占用了特定端口。
- **展示路由表信息**：显示内核中的 IP 路由表，包括目标网络、网关、子网掩码、接口等，类似 `route` 命令。

```bash
fan@FandeMac-mini ~ % netstat -nr
Routing tables

Internet:
Destination        Gateway            Flags               Netif Expire
default            192.168.2.1        UGScg                 en1       
127                127.0.0.1          UCS                   lo0       
```

- **统计网络接口数据**：显示网络接口的收发数据包数量、错误数和丢包数等信息。

```bash
fan@FandeMac-mini ~ % netstat -i
Name       Mtu   Network       Address            Ipkts Ierrs    Opkts Oerrs  Coll
lo0        16384 fandemac-mi fe80:1::1            88196     -    88196     -     -
en1        1500  fandemac-mi fe80:f::10ca:d342  2683439     -  1123699     -     -
```

- **网络协议统计信息**：系统中各类网络协议（主要是 TCP、UDP、ICMP 等）的详细统计数据，帮助分析网络连接状态、排查网络问题。

```bash
fan@192 ~ % netstat -s
Tcp:
    52321 active connections openings
    121 passive connection openings
    345 failed connection attempts
    123 connection resets received
    0 connections established
    123456 segments sent
    789012 segments received
    567 segments retransmitted  # TCP 重传数
    ...
Udp:
    9876 packets received
    123 packet receive errors
    8765 packets sent
    ...
Icmp:
    456 ICMP messages received
    123 echo requests (ping)
    123 echo replies
    ...
```



## 查看网络连接端口

使用本机的 SSH 连接后发现存在 ESTABLISHED 状态的连接：

```bash
fan@FandeMac-mini ~ % ssh 127.0.0.1\
> ;
(fan@127.0.0.1) Password:
Last login: Fri Aug  8 15:32:57 2025

fan@FandeMac-mini ~ % netstat -ant
Active Internet connections (including servers)
Proto Recv-Q Send-Q  Local Address          Foreign Address        (state)
tcp4       0      0  127.0.0.1.22           127.0.0.1.56642        ESTABLISHED
tcp4       0      0  127.0.0.1.56642        127.0.0.1.22           ESTABLISHED
tcp4       0      0  *.22                   *.*                    LISTEN     
tcp6       0      0  *.22                   *.*                    LISTEN     
```

TCP 依靠连接双方的 IP 地址和端口号共同构成连接节点，只有 LISTEN 状态能够接收连接请求 SYN 报文，处于 ESTABLISHED 状态的节点不能接收 SYN 报文。
