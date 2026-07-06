# DNS 中继服务器课程设计说明

## 项目结构

- `main.c`
  程序入口，只负责初始化并进入服务循环。
- `DNS_config.c / DNS_config.h`
  负责启动配置、命令行参数解析，以及当前工作目录下 `dnsrelay.txt` 的加载。
- `DNS_server.c / DNS_server.h`
  负责 UDP Socket 初始化、客户端请求接收、远程 DNS 转发与响应回传。
- `DNS_convert.c / DNS_convert.h`
  负责 DNS 报文解析与本地响应报文组装。
- `DNS_Hash.c / DNS_Hash.h`
  负责本地域名表的哈希存储与查询。
- `DNS_cache.c / DNS_cache.h`
  负责运行时缓存，减少重复查询。
- `ResetID.c / ResetID.h`
  负责并发转发时的 DNS ID 映射与超时控制。
- `dnsrelay.txt`
  本地域名表，格式为 `IP地址 域名`。

## 实现方式

程序启动后会先读取 `dnsrelay.txt`，把每条“域名 -> IP”规则装入哈希表，然后监听本机 `UDP 53` 端口。

当前版本采用单 socket 结构：程序只创建一个 UDP socket，它同时负责接收客户端请求、把未命中的请求转发给上游 DNS、接收上游 DNS 响应并回传给原客户端。

当客户端发来 DNS 查询后，程序按下面的顺序处理：

1. 解析 DNS 报文，取出查询域名、查询类型和原始 ID。
2. 如果不是 `A` 记录查询，则直接转发给远程 DNS。
3. 如果是 `A` 记录查询，则先查运行时缓存。
4. 缓存未命中时，再查本地哈希表。
5. 本地命中普通 IP 时，在本地直接构造 DNS 响应并返回。
6. 本地命中 `0.0.0.0` 时，按题目要求返回 `NXDOMAIN`。
7. 本地未命中时，为请求分配新的中继 ID，并转发给远程 DNS。
8. 远程 DNS 响应回来后，根据中继 ID 恢复原始 ID 与原客户端地址，再转发回去。
9. 对超时或迟到的 UDP 响应直接丢弃，避免错包。

说明：当前代码只检查当前工作目录中是否存在 `dnsrelay.txt`。如果当前目录下没有该文件，程序会直接报错退出，不会自动去别的目录查找。

## 命令语法

当前程序支持：

```text
dnsrelay [-d | -dd] [dns-server-ipaddr] [filename]
```

默认值：

- 默认上游 DNS：`10.3.9.5`
- 默认配置文件：当前目录下的 `dnsrelay.txt`

兼容用法：

- 仍然支持 `-s [server_address]` 指定上游 DNS

## 调试等级

### `-d`

一级调试，只输出：

- 时间戳
- 查询序号
- 查询域名

示例输出形式：

```text
12:34:56.789 1 www.baidu.com
```

### `-dd`

二级调试，会输出：

- 时间戳、查询序号、查询域名
- DNS 报文头部解析信息
- Question / Answer 详细解析
- 缓存命中与未命中信息
- 本地表命中信息
- 请求转发到远程 DNS 的信息
- 远程响应接收信息

## 常用启动命令

### 默认启动

```powershell
cd D:\zhzj\source\repos\computer_net\computer_net
..\x64\Debug\computer_net.exe
```

### 一级调试

```powershell
..\x64\Debug\computer_net.exe -d
```

### 二级调试

```powershell
..\x64\Debug\computer_net.exe -dd
```

### 指定上游 DNS

```powershell
..\x64\Debug\computer_net.exe 10.3.9.5
..\x64\Debug\computer_net.exe -s 10.3.9.5
```

### 指定上游 DNS 与配置文件

```powershell
..\x64\Debug\computer_net.exe -dd 10.3.9.5 dnsrelay.txt
..\x64\Debug\computer_net.exe -d 10.3.9.4 c:\dns-table.txt
```

## 编译命令

```powershell
& 'D:\zhzj\Program File\Microsoft Visual Studio\2026\Community\MSBuild\Current\Bin\MSBuild.exe' `
  'D:\zhzj\source\repos\computer_net\computer_net.slnx' `
  /t:Build /p:Configuration=Debug /p:Platform=x64
```

## 常用测试命令

```powershell
nslookup www.bupt.edu.cn 127.0.0.1
nslookup www.baidu.com 127.0.0.1
nslookup -type=AAAA www.baidu.com 127.0.0.1
ipconfig /displaydns
ipconfig /flushdns
ipconfig /all
```

## 与 PPT 要求的对应关系

| PPT 要求 | 当前状态 | 说明 |
| --- | --- | --- |
| 读取 `IP地址-域名` 对照表 | 已实现 | 启动时读取 `dnsrelay.txt` |
| 普通 IP 直接本地返回 | 已实现 | 本地命中后直接组装 `A` 记录响应 |
| `0.0.0.0` 返回域名不存在 | 已实现 | 本地命中 `0.0.0.0` 时返回 `NXDOMAIN` |
| 本地未命中则转发远程 DNS | 已实现 | 使用同一个 `dnsSocket` 转发到上游 DNS |
| 支持多客户端并发查询 | 已实现 | 通过 `ID_list` 同时维护多条挂起请求 |
| 使用 DNS ID 进行映射转换 | 已实现 | 转发前改写 ID，响应时恢复原始 ID |
| 处理 UDP 超时与迟到响应 | 已实现 | 通过 `expire_time` 判断并丢弃过期响应 |
| 按协议解析 DNS 报文 | 已实现 | 已解析 Header / Question / Answer |
| 组装本地 DNS 响应 | 已实现 | 本地命中时由程序直接编码响应 |
| 监听本地 UDP 53 端口 | 已实现 | `dnsSocket` 绑定本地 53 端口 |

## 测试方案

### 1. 本地普通记录命中测试

假设 `dnsrelay.txt` 中存在：

```text
123.127.134.10 bupt
11.111.11.111 test1
```

执行：

```powershell
nslookup bupt 127.0.0.1
nslookup test1 127.0.0.1
```

预期结果：

- 直接返回本地表中的 IPv4 地址。

### 2. 本地拦截测试

假设 `dnsrelay.txt` 中存在：

```text
0.0.0.0 008.cn
0.0.0.0 www.126p.com
```

执行：

```powershell
nslookup 008.cn 127.0.0.1
nslookup www.126p.com 127.0.0.1
```

预期结果：

- 返回域名不存在，即 `NXDOMAIN`。

### 3. 远程 DNS 转发测试

执行：

```powershell
nslookup www.bupt.edu.cn 127.0.0.1
nslookup www.baidu.com 127.0.0.1
```

预期结果：

- 本地表未命中。
- 请求被转发到远程 DNS。
- 正确返回远程解析结果。

### 4. 非 A 记录转发测试

执行：

```powershell
nslookup -type=AAAA www.baidu.com 127.0.0.1
nslookup -type=MX bupt.edu.cn 127.0.0.1
```

预期结果：

- 不在本地直接构造响应。
- 请求转发给远程 DNS。
- 返回远程 DNS 的结果。

### 5. 缓存测试

执行：

```powershell
cd D:\zhzj\source\repos\computer_net\computer_net
..\x64\Debug\computer_net.exe -dd
nslookup www.baidu.com 127.0.0.1
nslookup www.baidu.com 127.0.0.1
```

预期结果：

- 第二次查询更容易直接命中缓存。
- 调试输出中可以看到缓存命中提示。

### 6. 并发查询测试

快速连续执行：

```powershell
nslookup www.baidu.com 127.0.0.1
nslookup www.bupt.edu.cn 127.0.0.1
nslookup www.qq.com 127.0.0.1
nslookup test1 127.0.0.1
```

预期结果：

- 多个请求都能正常返回。
- 不会把远程响应发给错误的客户端。

### 7. 整机实验步骤

1. 先记录当前真实 DNS 服务器：

```powershell
ipconfig /all
```

当前活动 `WLAN` 的 DNS 服务器顺序为：

- `10.3.9.5`
- `10.3.9.4`
- `10.3.9.6`

因此默认上游 DNS 可优先使用 `10.3.9.5`。

2. 将网卡 DNS 临时设置为：

```text
127.0.0.1
```

3. 启动程序：

```powershell
cd D:\zhzj\source\repos\computer_net\computer_net
..\x64\Debug\computer_net.exe -d
```

如果默认上游 DNS `10.3.9.5` 不可用，再改成：

```powershell
..\x64\Debug\computer_net.exe -d 10.3.9.4
```

4. 再执行：

```powershell
ping www.baidu.com
nslookup www.bupt.edu.cn
ipconfig /displaydns
ipconfig /flushdns
```

预期结果：

- 系统可以通过本地 DNS 中继程序正常完成名字解析。
- 本地拦截项会返回域名不存在。
- 远程解析项会经由本程序完成转发。

## 概念说明

### DNS

DNS 是 Domain Name System，用来把域名转换成 IP 地址。

### 域名

域名是主机或服务在网络中的名字，例如 `www.baidu.com`。

### IP 地址

IP 地址是网络中主机的逻辑地址，例如 `127.0.0.1`、`8.8.8.8`。

### DNS 服务器

DNS 服务器负责接收域名查询请求并返回解析结果。

### DNS 中继服务器

DNS 中继服务器位于客户端和远程 DNS 服务器之间。它会先查本地规则，无法回答时再转发给上游 DNS。

### 本地域名表

本地域名表就是 `dnsrelay.txt` 中维护的“域名 -> IP”映射规则。

### 远程 DNS 服务器

远程 DNS 服务器就是本地程序无法直接回答时要转发过去的上游 DNS。

### DNS 查询

DNS 查询是客户端请求某个域名解析结果的过程。

### 递归查询

递归查询是指客户端把“得到最终结果”的责任交给 DNS 服务器。

### 迭代查询

迭代查询是指服务器无法直接给出最终答案时，只返回下一步应该去问谁。

### DNS 缓存

DNS 缓存用于保存已经得到的解析结果，以减少重复查询。

### TTL

TTL 是 Time To Live，表示 DNS 记录可以在缓存中保留多久。

### DNS 报文

DNS 报文是 DNS 协议中客户端和服务器之间交换的数据格式。

### Header

Header 是 DNS 报文头，包含 ID、标志位以及各段记录数。

### Question

Question 表示客户端要查询的内容，通常包含域名、查询类型和查询类。

### Answer

Answer 表示 DNS 服务器返回的解析结果。

### Resource Record

Resource Record，简称 RR，是 DNS 中表示资源信息的记录单元。

### A 记录

A 记录表示域名对应的 IPv4 地址。

### AAAA 记录

AAAA 记录表示域名对应的 IPv6 地址。

### MX 记录

MX 记录表示邮件交换服务器信息。

### CNAME 记录

CNAME 记录表示别名记录。

### ID

ID 是 DNS 请求和响应的匹配标识。本项目中会把客户端原始 ID 临时替换成中继内部 ID，再在响应返回时恢复。

### NXDOMAIN

NXDOMAIN 表示域名不存在。本项目用它表示本地表中 `0.0.0.0` 的拦截结果。

### UDP

UDP 是无连接传输协议。普通 DNS 查询通常通过 UDP 53 端口完成。

### 并发查询

并发查询是指多个客户端请求在时间上交错到达服务器，程序必须正确区分每个请求和响应的对应关系。

### 超时与迟到响应

由于 UDP 不保证可靠到达，远程 DNS 可能不响应，也可能响应过晚。因此程序要为转发请求设置过期时间，并丢弃迟到响应。

### 哈希表

哈希表是一种适合精确查找的键值映射结构。本项目用它保存本地域名表，并用链表解决哈希冲突。

### LRU 缓存

LRU 是 Least Recently Used，表示最近最少使用。缓存满时，优先淘汰最近最少访问的记录。
