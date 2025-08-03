# ifconfig - 网络接口信息

`ifconfig`（interface configuration）是一款用于配置和查看网络接口信息的命令行工具，它可以显示网络接口的 IP 地址、MAC 地址、子网掩码等信息，也能临时配置网络参数（如启用 / 禁用接口、设置 IP 地址等）。

[TOC]

## 核心功能

1. **查看网络接口信息**：显示所有或指定网络接口的详细参数（IP 地址、MAC 地址、状态等）。
2. **配置网络接口**：临时设置 IP 地址、子网掩码、广播地址等。
3. **管理接口状态**：启用（激活）或禁用（关闭）网络接口。
4. **设置 MTU、硬件地址（MAC）**：调整最大传输单元、临时 MAC 地址（需特定权限）。

## 基本用法

### 查看所有网络接口信息

```bash
ifconfig
```

输出所有网络接口的详细信息。

```bash
en1: flags=8863<UP,BROADCAST,SMART,RUNNING,SIMPLEX,MULTICAST> mtu 1500
	options=6460<TSO4,TSO6,CHANNEL_IO,PARTIAL_CSUM,ZEROINVERT_CSUM>
	ether 16:df:f4:48:07:a2
	inet6 fe80::d6:704b:1dc1:9442%en1 prefixlen 64 secured scopeid 0xf 
	inet 192.168.2.120 netmask 0xffffff00 broadcast 192.168.2.255
	inet6 240e:3af:e0b:18b0:148c:9e4f:adb4:a37c prefixlen 64 autoconf secured 
	inet6 240e:3af:e0b:18b0:6d8e:c49c:fd55:c1d1 prefixlen 64 autoconf temporary 
	nd6 options=201<PERFORMNUD,DAD>
	media: autoselect
	status: active
```

- flags=8633：状态标记位，表示接口属性：
  - `UP`：接口已启用（软件层面激活）。
  - `BROADCAST`：支持广播通信（可发送 / 接收广播包）。
  - `SMART`：支持自动协商（如速率自适应，常见于现代网络接口）。
  - `RUNNING`：接口已连接到物理网络（有链路信号，如 Wi-Fi 已关联、网线已插好）。
  - `SIMPLEX`：单工模式（此处可能为系统默认标注，实际网络通常为全双工）。
  - `MULTICAST`：支持组播通信（可发送 / 接收组播包）。
- **`mtu 1500`**：最大传输单元为 1500 字节（以太网和 Wi-Fi 的标准值，决定单次可传输的最大数据包大小）。
- 接口选项：
  - `TSO4`/`TSO6`：TCP 分段卸载（TCP Segmentation Offload），分别支持 IPv4 和 IPv6。由网卡硬件完成 TCP 数据包分段，减轻 CPU 负担，提升传输效率。
  - `CHANNEL_IO`：通道 I/O， macOS 针对 Apple 芯片优化的 I/O 处理机制，提升网络性能。
  - `PARTIAL_CSUM`：部分校验和，允许硬件仅计算数据包部分内容的校验和，平衡性能与可靠性。
  - `ZEROINVERT_CSUM`：校验和零反转，用于特定场景下的校验和计算修正（底层硬件优化）。
- 硬件地址：ether 16:df:f4:48:07:a2 表示接口的 MAC 地址。
- IP 地址信息：
  - **`inet 192.168.2.120`**：局域网 IPv4 地址（私网地址，192.168.x.x 网段）。
  - **`netmask 0xffffff00`**：子网掩码，换算为十进制是`255.255.255.0`，表示子网内可容纳 254 台设备（192.168.2.1 ~ 192.168.2.254）。
  - **`broadcast 192.168.2.255`**：子网广播地址，向该地址发送的数据包会被子网内所有设备接收。
- `media: autoselect`：接口自动选择通信介质和速率（如 Wi-Fi 自动协商信道和带宽，以太网自动识别 100M/1G 速率）。
- `status: active`：接口处于完全活跃状态 —— 已连接网络、获取 IP 地址、可正常收发数据。

## 常见网络接口

### 物理接口

- **以太网接口（Ethernet）**：最常用的有线网络接口，通常以 `enX`（如 `en0`、`en1`）命名（Linux/macOS）或 `Ethernet` 标识（Windows）。支持双绞线（RJ45 接口）连接，速率从 10Mbps 到 100Gbps 不等，用于连接局域网（LAN）、交换机、路由器等。
- **无线局域网接口（Wi-Fi）**：无线连接接口，通常以 `wlX`（如 `wlan0`、`en1`，部分系统中无线接口也可能用 `en` 前缀）命名，通过无线电波（2.4GHz/5GHz/6GHz 频段）连接无线路由器，支持 802.11a/b/g/n/ac/ax 等协议。
- **蓝牙接口（Bluetooth）**：短距离无线接口，主要用于设备间近距离通信（如连接耳机、键盘、手机等），通常以 `bt` 或 `bluetooth` 相关名称标识。
- **光纤接口**：通过光纤传输数据的高速接口，常见于服务器、数据中心或长距离传输场景，速率可达 10Gbps 以上，接口类型包括 LC、SC 等。
- **调制解调器接口（Modem）**：用于连接电话线（ADSL）、有线电视线（Cable Modem）或光纤猫（PON），实现接入互联网，通常以 `modem` 或 `ppp`（点对点协议）相关标识。
- **USB 网络接口**：通过 USB 端口扩展的网络接口（如 USB 转以太网、USB 无线网卡），通常会被系统识别为独立的网络接口（如 `enx+MAC地址`）。

### 逻辑接口

- **回环接口（Loopback）**：通常命名为 `lo` 或 `lo0`，IP 地址固定为 `127.0.0.1`（IPv4）和 `::1`（IPv6），用于本机内部进程间通信（如本地服务访问 `localhost`），不依赖物理网络。
- **隧道接口（Tunnel）**：用于封装数据包在不同网络间传输（如跨网段、跨协议），常见类型包括：
  - `gif`：通用路由封装（GRE）隧道接口，支持 IPv4/IPv6 封装。
  - `tun`/`tap`：虚拟点对点隧道接口，`tun` 用于三层 IP 数据包，`tap` 用于二层以太网帧，常用于 VPN（如 OpenVPN）。
  - `ipip`：IPv4 over IPv4 隧道接口。
- **桥接接口（Bridge）**：通常命名为 `br0` 等，用于将多个网络接口 “桥接” 成一个逻辑网络（如虚拟机桥接模式，让虚拟机直接接入物理网络）。
- **VLAN 接口**：基于 VLAN（虚拟局域网）的逻辑接口，通常以 `eth0.100`（表示接口 `eth0` 上的 VLAN 100）命名，用于在单一物理接口上划分多个隔离的逻辑网络。
