# TCP 协议

TCP（Transmission Control Protocol，传输控制协议）是一种面向连接的、可靠的、基于字节流的传输层协议，由 IETF 的 RFC 793 定义。它在 IP 协议的基础上，提供了**可靠数据传输**、**流量控制**和**拥塞控制**等核心功能.。

[TOC]

## 协议报文

TCP 报文的基本头部固定 20 个字节，紧跟在 IP 数据报头部。

```shell
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-------------------------------+-------------------------------+
   |          Source Port          |       Destination Port        |
   +-------------------------------+-------------------------------+
   |                        Sequence Number                        |
   +---------------------------------------------------------------+
   |                    Acknowledgment Number                      |
   +-------+-------+-+-+-+-+-+-+-+-+-------------------------------+
   |  Data |       |C|E|U|A|P|R|S|F|                               |
   | Offset|Rsved  |W|C|R|C|S|S|Y|I|            Window             |
   |       |       |R|E|G|K|H|T|N|N|                               |
   +-------+-----------+-+-+-+-+-+-+-------------------------------+
   |           Checksum            |         Urgent Pointer        |
   +-------------------------------+-------------------------------+
   |                            Options                            |
   +---------------------------------------------------------------+
   |                             data                              |
   +---------------------------------------------------------------+
```

源端口和目的端口唯一标识 TCP 连接。

- Socket：IP + 端口
- 序列号：标识数据的每个字节序列
- 确认号：接收方期待接收的下一个序列号

## 核心特性

- 面向连接：通过三次握手、四次挥手管理连接建立和释放。
- 可靠传输：序列号、确认机制、重传机制、校验和。
- 字节流：

### 面向连接

为了确保通信双方都能正常发送和接收数据，TCP 连接通过“三次握手”来建立连接，通过“四次挥手”释放连接。

#### 三次握手



### 可靠传输



## 拥塞控制

拥塞控制是 TCP 协议中用于**避免网络拥塞**、保证数据传输效率的关键机制，核心是通过**动态调整发送窗口大小**，平衡网络吞吐量和稳定性。

> 网络拥塞：发送方数据速率超过网络承载能力，导致路由器缓存溢出、丢包、延迟剧增的现象。

- **拥塞窗口 cwnd**：发送方维护的动态窗口，反应当前网络可承载的最大数据量（单位：MSS，最大报文段长度）。
- **慢启动阈值 ssthresh**：区分 “慢启动” 和 “拥塞避免” 阶段的临界值。初始值通常为较大值（如 65535 字节）。
- **实际发送窗口**：发送方的实际发送速率由 `min(cwnd, rwnd)` 决定，其中 `rwnd` 是接收方通过 TCP 头部 “窗口字段” 告知的接收窗口（流量控制，与接收缓冲区大小相关）。

### 核心算法

TCP 拥塞控制通过**慢启动、拥塞避免、快速重传、快速恢复**四个阶段实现网络拥塞程度的探测和调整。

#### 慢启动

- 连接建立后，`cwnd` 从 1 个 MSS 开始，每收到一个 ACK（确认）就增加 1 个 MSS（指数增长）。
- 当 `cwnd` 达到 `ssthresh` 时，进入 “拥塞避免” 阶段。

```cpp
static void bictcp_cong_avoid(struct sock *sk, u32 ack, u32 acked)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct bictcp *ca = inet_csk_ca(sk);

	// 判断是否受拥塞控制，RFC2861：只有当 cwnd 完全利用时才增加大小
	if (!tcp_is_cwnd_limited(sk))
		return;

	// 判断是否在慢启动阶段
	// 慢启动的条件是：发送窗口 send_cwnd < ssthresh
	if (tcp_in_slow_start(tp)) {
		if (hystart && after(ack, ca->end_seq))
			bictcp_hystart_reset(sk);
		acked = tcp_slow_start(tp, acked);
		if (!acked)
			return;
	}
	bictcp_update(ca, tp->snd_cwnd, acked);
	tcp_cong_avoid_ai(tp, ca->cnt, acked);
}

u32 tcp_slow_start(struct tcp_sock *tp, u32 acked)
{
  // cwnd 没个 ack 增加 1 个 MSS，但是不能超过 ssthresh
	u32 cwnd = min(tp->snd_cwnd + acked, tp->snd_ssthresh);
	// 与旧窗口比较，防止增长超过 ssthresh
	acked -= cwnd - tp->snd_cwnd;
	tp->snd_cwnd = min(cwnd, tp->snd_cwnd_clamp);

	return acked;
}
```

#### 拥塞避免

- `cwnd` 不再指数增长，而是每经过一个 RTT（往返时间）增加 1 个 MSS（线性增长），避免快速触发拥塞。

CUBIC 算法中，窗口增长基于三次函数，其核心是快速接近上次拥塞的窗口 last_max_cwnd，同时避免频繁拥塞。

#### 拥塞检测与响应

当检测到拥塞（丢包或延迟增加）时，调整 `ssthresh` 和 `cwnd`：

- **超时重传**：认为拥塞严重，`ssthresh = cwnd / 2`，`cwnd` 重置为 1（重新进入慢启动）。
- **收到 3 个重复 ACK**：认为发生轻微拥塞（可能仅单包丢失），触发 “快速重传” 和 “快速恢复”。