# IS-IS 协议

中间系统到中间系统IS-IS（Intermediate System to Intermediate System）属于内部网关协议IGP（Interior Gateway Protocol），用于自治系统内部。IS-IS也是一种链路状态协议，使用最短路径优先SPF（Shortest Path First）算法进行路由计算。

[TOC]

## 基本概念

IS-IS 协议支持大规模的路由网络，采用骨干区域和非骨干区域两级分层结构。

![](../Pics/isis_area.png)

Level-1 路由器部署在非骨干区域，Level-2 路由器和 Level-1-2 路由器部署在骨干区域。每一个非骨干区域都通过 Level-1-2 路由器与骨干区域相连。

- 每个路由器都只属于一个区域。OSPF 中一个路由器的不同接口可以属于不同区域。
- 单个区域没有骨干和非骨干区域的概念。OSPF 中 Area 0 定义为骨干区域。
- Level-1 和 Level-2 级别的路由都采用 SPF 算法，分别生成最短路径树SPT。OSPF 中只有在同一个区域内才使用 SPF 算法，区域之间的路由需要通过骨干区域来转发。

### 路由器分类

- Level-1 路由器：负责区域内路由。
  - 同一区域的 Level-1 和 Level-1-2 路由器形成邻居关系。
  - 只负责维护 Level-1 的 LSDB，包含本区域的路由信息。
- Level-2 路由器：负责区域间的路由。
  - 与同一区域或不同区域的 Level-2 路由器，也可以与同一区域或不同区域的 Level-1-2 路由器形成邻居关系。
  - 维护的 LSDB 包含区域间的路由信息。
  - 形成 Level-2 邻居关系的路由器组成骨干网，必须是物理连接保证骨干网的连续性。
  - 只有 Level-2 路由器才能直接与区域外的路由器交换数据报文或路由信息。
- Level-1-2 路由器：同时属于 Level-1 和 Level-2 路由器。
  - 可以与同一区域的 Level-1 和 Level-2 路由器形成 Level-1 邻居关系，也可以与同一区域或不同区域的 Level-2 和其他区域的 Level-1-2 形成 Level-2 邻居关系。
  - 维护两个 LSDB，分别用于区域内路由和区域间路由。

### DIS 和伪节点

IS-IS 在所有路由器中选举一个作为 DIS，用来创建和更新伪节点（Pseudonode），负责生成伪节点的链路状态协议数据单元 LSP，描述网络上的网络设备。

伪节点：模拟广播网络的虚拟节点，使用 DIS 的 System ID 和 Circuit 标识。

