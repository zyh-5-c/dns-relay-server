# DNS 中继服务器 / DNS 代理实验

一个 Windows 平台 DNS 中继服务器课程实验，基于 C 语言和 UDP Socket 实现。程序监听本地 `53` 端口，优先查询本地域名表和运行时缓存，未命中时转发到上游 DNS，并通过请求 ID 映射完成响应回传。

## 项目特点

- 使用 UDP Socket 监听本地 `53` 端口，处理客户端 DNS 查询请求。
- 支持 `dnsrelay.txt` 本地域名表查询、LRU 缓存命中、上游 DNS 转发 3 类解析场景。
- 解析 DNS Header / Question / Answer 字段，支持 A 记录与 PTR 反向查询。
- 对非 A 查询保留报文结构并转发，保证基础兼容性。
- 设计请求 ID 映射表，维护客户端地址、原始 ID、转发 ID 和请求上下文，解决上游响应和客户端请求的匹配问题。
- Debug 模式输出 DNS 报文 ID、flags、rcode、查询域名等信息，便于抓包和调试。

## 目录结构

```text
.
├── main.c              # 程序入口
├── DNS_config.*        # 参数解析和配置加载
├── DNS_server.*        # UDP Socket、请求转发与响应回传
├── DNS_convert.*       # DNS 报文解析与响应构造
├── DNS_Hash.*          # 本地域名表哈希查询
├── DNS_cache.*         # 运行时缓存
├── ResetID.*           # DNS 请求 ID 映射
├── DNS_print.*         # 调试信息输出
├── DNS_struct.h        # DNS 报文结构
├── default.h           # 常量配置
└── dnsrelay.txt        # 本地域名表
```

## 运行方式

程序需要以管理员权限运行，才能监听本地 `53` 端口。

```text
dnsrelay [-d | -dd] [dns-server-ipaddr] [filename]
```

示例：

```text
dnsrelay
dnsrelay -d 8.8.8.8 dnsrelay.txt
dnsrelay -dd 10.3.9.5 dnsrelay.txt
```

## GitHub 上传说明

本仓库只保留源码、配置文件和说明文档，不提交 `.exe`、`.obj`、课程报告、IDE 缓存等本地文件。`dnsrelay.txt` 为本地域名表样例，可按实验需要自行替换。
