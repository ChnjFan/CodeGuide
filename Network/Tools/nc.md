# nc

**nc** 即 netcat 命令，这个工具在排查网络故障时非常有用，功能非常强大，因而被业绩称为网络界的“瑞士军刀”。

[TOC]

## 模拟服务器客户端

**nc** 命令常见的用法是模拟一个服务器程序被其他客户端连接，或者模拟一个客户端连接其他服务器，连接之后就可以进行数据收发。

### 模拟服务器

使用 **-l** 选项（单词 **l**isten 的第一个字母）在某个 ip 地址和端口号上开启一个侦听服务，以便让其他客户端连接。通常为了显示更详细的信息，会带上 **-v** 选项。

```bash
root@iZf8z4o3lbylu7kddpzwa1Z:~# nc -v -l 127.0.0.1 6000
Listening on localhost 6000
```

### 模拟客户端

用 **nc** 命令模拟一个客户端程序时，我们不需要使用 **-l** 选项，直接写上 ip 地址（或域名，**nc** 命令可以自动解析域名）和端口号即可。

```bash
root@iZf8z4o3lbylu7kddpzwa1Z:~# nc -v www.baidu.com 80
Connection to www.baidu.com (183.2.172.17) 80 port [tcp/http] succeeded!
```

使用 **nc** 命令作为客户端时可以使用 **-p** 选项指定使用哪个端口号连接服务器。

```bash
root@iZf8z4o3lbylu7kddpzwa1Z:~# nc -v -p 5555 www.baidu.com 80
Connection to www.baidu.com (183.2.172.17) 80 port [tcp/http] succeeded!
```

