# OSPF 协议

OSPF 开放式最短路径优先是基于**链路状态**的内部网关协议（IGP），在自治系统 AS 内部用于路由器之间交换信息并计算最优路径。

[TOC]

## 基本概念

OSPF 协议采用 SPF 算法（Dijkstra 算法），以链路状态数据库 LSDB 为基础生成路由表。

IPv4 协议使用 OSPF Version 2，IPv6 协议使用 OSPF Version 3。

### 核心特性

RIP 协议基于距离矢量算法的路由协议，存在着收敛慢、路由环路、可扩展性差等问题。

OSPF 基于链路状态的协议解决 RIP 的问题，应用于规模中等的网络，最多支持几百台设备。

核心特性包括：

- 采用**组播收发报文**，减少对其他不运行 OSPF 路由器的影响。
- **无类路由协议**，支持 VLSM 和 CIDR，能高效利用 IP 地址空间。
- **链路状态协议**，路由器仅交换直连链路的状态（接口 IP、带宽、开销）而非完整路由表。
- 区域划分，将 AS 划分为多个区域 Area，降低 LSDB 的规模和 SPF 计算复杂度。
- 路由开销基于接口带宽计算（bps），带宽越高开销越小。

### Router ID

Router ID是一个32比特无符号整数，是一台设备在自治系统中的唯一标识。

可以手动配置和设备自动设定两种方式，设备从系统 ID 或者当前接口 IP 地址自动选择。

### 报文类型

OSPF 使用 IP 报文封装协议报文，协议号为 89。

- Hello 报文：周期发送，发现和维持邻居关系。
- DD（Database Description packet）报文：描述本地LSDB（Link State Database）的摘要信息，两台设备数据库同步。
- LSR 报文：向对方请求所需的 LSA，OSPF 邻居交换 DD 报文后向对方发出 LSR 报文。
- LSU 报文：向对方发送需要的 LSA。
- LSAck 报文：对收到的 LSA 进行确认。

### 区域划分

大型网络运行 OSPF 路由协议时，设备数量增多会导致链路状态数据库 LSDB 庞大，运行 SPF 算法复杂度增加。网络规模增大拓扑结构容易发生变化，造成网络中有大量 OSPF 协议报文传递，降低网络带宽利用率。每次变化都会导致网络中所有设备重新计算路由。

OSPF 协议将 AS 划分为不同区域**解决 LSDB 频繁更新问题**，提交网络利用率。

区域是从逻辑上将设备划分为不同组。

- 区域号（Area ID）标识不同区域。
- 区域的边界是设备而不是链路。
- 一个网段（链路）只能属于一个区域，运行 OSPF 协议的接口必须指明区域。

区域类型：

- 普通区域：默认情况 OSPF 区域被定义为普通区域。
  - 标准区域：传输区域内路由，区域间路由和外部路由。
  - 骨干区域：连接所有其他 OSPF 区域的中央区域，Area 0 表示。负责区域之间的路由，非骨干区域之间的路由信息必须通过骨干区域转发。
- Stub 区域：位于 AS 的边界，只有一个 ABR 的非骨干区域。
  - ABR 不向区域内传播接收的 AS 外部路由，减少区域内设备的路由表规模和路由传递报文。
  - ABR 发送缺省路由给区域内其他设备，用于访问外部。
- NSSA 区域：非完全末梢区域是 Stub 区域的扩展。
  - ASBR 产生 Type7 LSA 携带外部路由信息在区域内传播。Type7 LSA 到达 ABR 时转换为 Type5 LSA 泛红到整个 OSPF 域。

### 设备角色

根据设备在 AS 中的不同位置，可以划分为不同角色。

![](../Pics/ospf_area.png)

- 区域内路由器：设备所有接口都属于同一个 OSPF 区域。
- 区域边界路由器 ABR：连接骨干区域和非骨干区域，同属于骨干区域和非骨干区域。
- 骨干路由器：至少一个接口属于骨干区域。ABR 和骨干区域内的路由器都属于骨干路由器。
- AS 边界路由器 ASBR：与其他 AS 交换路由信息，可能是区域内路由器，也可能是 ABR。

OSPF 定义指定路由器 DR 和备份指定路由器 BDR，减少网络中路由器之间建立邻接关系的数量。

利用 Hello 报文中携带的优先级和 Router ID 选举 DR，所有路由器都将信息发给 DR，由 DR 将 LSA 广播出去，除 DR 和 BDR 之外的路由器之间不建立邻接关系。

### 链路状态通告 LSA

OSPF 对路由信息的描述封装在链路状态通告 LSA（Link State Advertisement）中发布出去，通过泛洪同步网络拓扑信息。

- Router-LSA（Type1）：每个设备都会产生，描述设备的链路状态和开销，在设备所在区域内传播。
- Network-LSA（Type2）：DR 产生，描述本网段状态，在 DR 所在区域传播。
- Network-summary-LSA（Type3）： ABR 产生，描述区域内某个网段的路由，并通告给其他区域。
- ASBR-summary-LSA（Type4）：ABR 产生，描述本区域到其他区域中的 ASBR 的路由，通告给除 ASBR 所在区域的其他区域。
- AS-external-LSA（Type5）：ASBR 产生，描述 AS 外部路由，通告到所有区域。
- NSSA LSA（Type7）：ASBR 产生，描述 AS 外部路由，在 NSSA 区域内传播。

### 邻居状态机

OSPF 网络中交换路由信息，邻居设备之间要建立邻接关系。

- 邻居关系：OSPF 设备启动后向外发 Hello 报文，收到 Hello 报文检查参数双方一致后建立邻居关系。
- 邻接关系：形成邻居关系后设备成功交换 DD 报文和 LSA 之后建立邻接关系。

OSPF共有8种状态机：

![](../Pics/ospf_neighbor_state.png)

- Down：邻居会话的初始阶段，邻居失效时间间隔内未收到 Hello 报文。
- Attempt 状态：NBMA 网络中对端在邻居失效时间间隔超时前仍然没有回复 Hello 报文。
- Init：收到 Hello 报文后的状态。
- 2-way：收到 Hello 报文中包含自己的 Router ID，建立邻居关系。
- Exstart：开始协商主从关系，确定 DD 序列号。
- Exchange：主从关系协商完毕后开始交换 DD 报文。
- Loading：交换 DD 报文完成。
- Full：LSR 重传列表为空。

### OSPF 路由聚合

ABR 可以将相同前缀的路由信息聚合在一起，只发布一条路由到其他区域。

区域间通过路由聚合，可以减少路由信息，从而减小路由表的规模，提高设备的性能。

- ABR 聚合：ABR 将连续网段聚合成一个网段，生成 Type3 LSA 向其他区域发布。
- ASBR 聚合：本地设备是 ASBR 将引入的聚合地址范围内的 Type5 LSA 进行聚合。配置 NSSA 区域时，对引入的聚合地址范围内的 Type7 LSA 进行聚合。

### OSPF 缺省路由

OSPF 路由器只有对区域外的出口才能发布缺省路由 LSA。

- ABR 发布 Type3 缺省 LSA，指导区域内设备进行区域间报文转发。
- ASBR 发布 Type5 缺省 LSA 或 Type7 外部缺省 LSA，指导 AS 内设备进行 AS 外报文转发。

OSPF 路由器已经发布了缺省路由 LSA，不再学习其它路由器发布的相同类型缺省路由。

外部缺省路由（指导报文域外转发）的发布如果要依赖于其它路由，那么被依赖的路由不能是本 OSPF 路由域内的路由。域内路由的下一跳都指向域内，不能满足报文域外转发要求。

## 基本原理

OSPF 协议路由的计算过程：

1. 建立邻接关系
   - 发送 Hello 报文建立邻居关系。
   - 主从关系协商和 DD 报文交换。
   - 更新 LSA 完成 LSDB 同步。
2. 路由计算：采用 SPF 算法计算路由，达到路由快速收敛。

### 建立邻接关系

状态机变化中，建立邻接关系的条件：

- 与邻居的双向通信初次建立时。
- 网段中的 DR 和 BDR 发生变化时。

#### 广播网络建立邻接关系

![](../Pics/ospf_broadcast.png)

1. **建立邻居关系**

（1）Router A 接口使能 OSPF 协议后，从该接口使用组播地址 224.0.0.5 发送 Hello 报文。Router A 认为自己是 DR（1.1.1.1），不确定邻居是什么（Neighbors Seen = 0）。

（2）Router B 收到 Router A 的 Hello 报文，回应一个 Hello 报文给 Router A，在 Neighbors Seen 字段填入 Router A 的 Router ID（1.1.1.1），宣告 DR 是 Router B（DR = 2.2.2.2）。此时 Router B的邻居状态切换为 Init。

（3）Router A 收到 Router B 回应的 Hello 报文后，邻居状态机置为 2-way 状态。

如果两个路由器是非 DR，将停留在这一步。

2. **主从关系协商、DD 报文交换**

（1）Router A 发送一个 DD 报文宣称自己是 Master（MS = 1，Seq = X），报文不包含 LSA 的摘要，只是协商主从关系。

（2）Router B 收到 DD 报文，将邻居状态机改为 Exstart，回应一个 DD 报文（不包含 LSA 摘要，Seq = Y），自己的 Router ID 较大，在报文中认为自己是 Master，重新规定序列号。

（3）Router A 收到报文后同意 Router B 作为 Master，邻居状态机改为 Exchange，使用 Router B 发送回来的序列号发送新的 DD 报文，正式传送 LSA 的摘要（Seq = Y，MS = 0，Router A 是 Slave）。

（4）Router B 收到报文后将邻居状态机改为 Exchange，发送新的 DD 报文描述自己的 LSA 摘要（Seq = Y + 1）。

持续上述过程，当 Router B 发送最后一个 DD 报文后在报文中设置 M = 0。

3. **LSDB 同步（LSR、LSU、LSAck）**

（1）Router A 收到最后一个 DD 报文后，确定 Router B 数据库里自己没有的 LSA，邻居状态机改为 Loading 状态。Router B 收到最后一个 DD报文发现数据库都有 Router A 的 LSA，直接将领居状态机改为 Full 状态。

（2）Router A 发送 LSR 向 Router B 请求更新 LSA，Router B 回复 LSU，Router A 收到后发送 LSAck 确认。

一直持续到 LSA 完全同步，Router A 将领居状态机改为 Full 状态。

路由器交换完 DD 报文并更新 LSA 后邻接关系建立完成。

#### NBMA 网络建立邻接关系

NBMA 网络和广播网络建立区别是在邻居关系的建立。

![](../Pics/ospf_nbma.png)

Router B 向 Router A 发送 Hello 报文后，将邻居状态机改为 Attempt，Router B认为自己是 DR（DR = 2.2.2.2，Neighbors Seen = 0）。

Router A 收到 Hello 报文后将邻居状态机置为 Init，回复 Hello 报文。Router A 同意 Router B 是 DR（DR = 2.2.2.2， Neighbors Seen = 2.2.2.2）。

#### 点到点网络建立邻接关系

点到点网络不需要选举 DR 和 BDR，DD 报文通过组播发送。

### 路由计算

OSPF 采用 SPF（Shortest Path First）算法计算路由，快速收敛路由。

链路状态通告 LSA 描述路由器之间的链接和连接属性，路由器将 LSDB 转换为一张带权的有向图，描述整个网络拓扑结构。

每条路由器根据有向图使用 SPF 算法计算以自己为根节点的最短路径树，树中包含到 AS 中的各节点路由。

#### 计算区域内路由

Router LSA 与 Network LSA 描述区域内的网络拓扑，SPF 算法计算各路由器的最短路径。根据 LSA 描述与路由器的网段情况可以得到各网段具体路径。

#### 计算区域外路由

相邻区域的路由对应网段直接连接在 ABR，ABR 的路径在区域内计算完成，直接检查 Network Summary LSA 得到网段之间的最短路径。

ASBR 也可以看成连接在 ABR 上，同样的方式也可以计算出来。

#### 计算自治系统外路由

自治系统外部路由直接连接在 ASBR 上，逐条检查 AS External LSA 可以得到外部网络最短路径。

