# lsof

**lsof**（List Open Files）是 Linux/Unix 系统中用于**列出所有进程打开的文件**的命令行工具，基于 “一切皆文件” 的设计，可查看普通文件、目录、套接字、设备、管道等资源的占用情况。

[TOC]

## 使用方式

- 定位**谁在占用文件 / 端口**（如 “文件忙无法删除”“端口被占用”）
- 排查**磁盘空间异常**（已删文件仍被进程占用）
- 查看进程打开的文件、网络连接与资源

```bash
COMMAND     PID   TID    USER   FD      TYPE             DEVICE  SIZE/OFF       NODE NAME
systemd       1          root  cwd       DIR              202,1      4096          2 /
nscd        453   469    nscd    8u  netlink                          0t0      11017 ROUTE
nscd        453   470    nscd  cwd       DIR              202,1      4096          2 /
nscd        453   470    nscd  rtd       DIR              202,1      4096          2 /
nscd        453   470    nscd  txt       REG              202,1    180272     146455 /usr/sbin/nscd
nscd        453   470    nscd  mem       REG              202,1    217032     401548 /var/db/nscd/hosts
nscd        453   470    nscd  mem       REG              202,1     90664     132818 /usr/lib64/libz.so.1.2.7
nscd        453   470    nscd  mem       REG              202,1     68192     133155 /usr/lib64/libbz2.so.1.0.6
nscd        453   470    nscd  mem       REG              202,1    153192     133002 /usr/lib64/liblzma.so.5.0.99
nscd        453   470    nscd  mem       REG              202,1     91496     133088 
nscd        453   471    nscd    5u  a_inode                0,9         0       4796 [eventpoll]
nscd        453   471    nscd    6r      REG              202,1    217032     401548 /var/db/nscd/hosts
nscd        453   471    nscd    7u     unix 0xffff880037497440       0t0      11015 /var/run/nscd/socket
nscd        453   471    nscd    8u  netlink                          0t0      11017 ROUTE
imgserver   611       zhangyl  cwd       DIR              202,1      4096    1059054 /home/zhangyl/flamingoserver
imgserver   611       zhangyl  rtd       DIR              202,1      4096          2 /
imgserver   611       zhangyl  txt       REG              202,1   4788917    1057044 /home/zhangyl/flamingoserver/imgserver
imgserver   611       zhangyl   24u  a_inode                0,9         0       4796 [eventfd]
imgserver   611       zhangyl   25u     IPv4           55707643       0t0        TCP *:commtact-http (LISTEN)
imgserver   611       zhangyl   26r      CHR                1,3       0t0       4800 /dev/null
imgserver   611   613 zhangyl   32w      REG              202,1    131072    2754609 /home/zhangyl/flamingoserver/imgcache/258bfb8945288a117d98d440986d7a03
```

使用 **lsof** 命令有三点需要注意：

- 默认情况下，**lsof** 的输出比较多，我们可以使用 grep 命令过滤我们想要查看的进程打开的 fd 信息。或者使用 **lsof -p pid** 也能过滤出指定的进程打开的 fd 信息。
- **lsof** 命令只能查看到当前用户有权限查看到的进程 fd 信息，对于其没有权限的进程，最右边一列会显示 “**Permission denied**”。如下所示：
- **lsof** 命令第一栏进程名在显示的时候，默认显示前 n 个字符，这样如果我们需要显示完整的进程名以方便过滤的话，可以使用 **+c** 选项：`lsof +c 15`。

## 查看网络连接

socket 也是一种 fd，如果需要仅显示系统的网络连接信息，使用的是 **-i** 选项即可，这个选项可以形象地显示出系统当前的出入连接情况：

```bash
root@iZf8z4o3lbylu7kddpzwa1Z:~# lsof -i
COMMAND      PID            USER   FD   TYPE  DEVICE SIZE/OFF NODE NAME
systemd        1            root   77u  IPv4 8323555      0t0  TCP *:ssh (LISTEN)
systemd        1            root   78u  IPv6 8323557      0t0  TCP *:ssh (LISTEN)
AliYunDun 187355            root    9u  IPv4 6299446      0t0  TCP iZf8z4o3lbylu7kddpzwa1Z:47302->100.100.30.25:http (ESTABLISHED)
chronyd   753692         _chrony    5u  IPv4 7424446      0t0  UDP localhost:323 
chronyd   753692         _chrony    6u  IPv6 7424447      0t0  UDP ip6-localhost:323 
mysqld    782043           mysql   21u  IPv4 7688370      0t0  TCP localhost:33060 (LISTEN)
mysqld    782043           mysql   23u  IPv4 7688192      0t0  TCP localhost:mysql (LISTEN)
sshd      869253            root    3u  IPv4 8323555      0t0  TCP *:ssh (LISTEN)
sshd      869253            root    4u  IPv6 8323557      0t0  TCP *:ssh (LISTEN)
systemd-n 904041 systemd-network   22u  IPv4 8655451      0t0  UDP iZf8z4o3lbylu7kddpzwa1Z:bootpc 
systemd-r 904174 systemd-resolve   14u  IPv4 8656848      0t0  UDP _localdnsstub:domain 
systemd-r 904174 systemd-resolve   15u  IPv4 8656849      0t0  TCP _localdnsstub:domain (LISTEN)
systemd-r 904174 systemd-resolve   16u  IPv4 8656850      0t0  UDP _localdnsproxy:domain 
systemd-r 904174 systemd-resolve   17u  IPv4 8656851      0t0  TCP _localdnsproxy:domain (LISTEN)
sshd      925291            root    4u  IPv4 8819066      0t0  TCP iZf8z4o3lbylu7kddpzwa1Z:ssh->49.74.94.245:13589 (ESTABLISHED)
```

和 **netstat** 命令一样，**lsof -i** 默认也会显示 ip 地址和端口号的别名，我们只要使用 **-n** 和 **-P** 选项就能相对应地显示 ip 地址和端口号了，综合起来就是 **lsof -Pni**：

```bash
root@iZf8z4o3lbylu7kddpzwa1Z:~# lsof -Pni
COMMAND      PID            USER   FD   TYPE  DEVICE SIZE/OFF NODE NAME
systemd        1            root   77u  IPv4 8323555      0t0  TCP *:22 (LISTEN)
systemd        1            root   78u  IPv6 8323557      0t0  TCP *:22 (LISTEN)
AliYunDun 187355            root    9u  IPv4 6299446      0t0  TCP 172.31.158.79:47302->100.100.30.25:80 (ESTABLISHED)
chronyd   753692         _chrony    5u  IPv4 7424446      0t0  UDP 127.0.0.1:323 
chronyd   753692         _chrony    6u  IPv6 7424447      0t0  UDP [::1]:323 
mysqld    782043           mysql   21u  IPv4 7688370      0t0  TCP 127.0.0.1:33060 (LISTEN)
mysqld    782043           mysql   23u  IPv4 7688192      0t0  TCP 127.0.0.1:3306 (LISTEN)
sshd      869253            root    3u  IPv4 8323555      0t0  TCP *:22 (LISTEN)
sshd      869253            root    4u  IPv6 8323557      0t0  TCP *:22 (LISTEN)
systemd-n 904041 systemd-network   22u  IPv4 8655451      0t0  UDP 172.31.158.79:68 
systemd-r 904174 systemd-resolve   14u  IPv4 8656848      0t0  UDP 127.0.0.53:53 
systemd-r 904174 systemd-resolve   15u  IPv4 8656849      0t0  TCP 127.0.0.53:53 (LISTEN)
systemd-r 904174 systemd-resolve   16u  IPv4 8656850      0t0  UDP 127.0.0.54:53 
systemd-r 904174 systemd-resolve   17u  IPv4 8656851      0t0  TCP 127.0.0.54:53 (LISTEN)
sshd      925291            root    4u  IPv4 8819066      0t0  TCP 172.31.158.79:22->49.74.94.245:13589 (ESTABLISHED)
```

