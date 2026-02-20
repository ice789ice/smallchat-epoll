# smallchat-epoll

一个基于 `epoll` 的终端聊天室示例，当前版本使用：

- Reactor 架构
- ET（Edge Trigger）触发模式
- 非阻塞 socket
- 单线程事件循环

项目支持：

- 多客户端连接
- 首条消息作为昵称注册
- 后续消息广播给其他在线用户
- `/quit` 主动退出

---

## 1. 项目结构

```text
main.cpp
Server.h / Server.cpp
EventLoop.h / EventLoop.cpp
Channel.h / Channel.cpp
Connection.h / Connection.cpp
client.cpp
```

职责划分：

- `EventLoop`：封装 `epoll_wait` 主循环和 `epoll_ctl` 注册/删除。
- `Channel`：封装 fd 与事件（`EPOLLIN | EPOLLET`）以及 `readCallback`。
- `Server`：负责监听 socket、accept 新连接、连接状态管理、聊天协议处理。
- `Connection`：封装单连接的读写接口（读 chunk / 发送消息 / 连接关闭状态）。
- `client.cpp`：终端客户端，`select` 同时监听标准输入和 socket。

---

## 2. 聊天协议

服务端按“行”处理消息（以 `\n` 作为分隔）：

1. 第 1 行：昵称  
2. 第 2 行开始：普通聊天消息（广播给其他客户端）
3. 输入 `/quit`：广播离开消息并断开连接

示例：

```text
alice
hello everyone
/quit
```

服务端广播格式：

- 加入：`[system]: <nickname> joined`
- 聊天：`[<nickname>]: <message>`
- 离开：`[system]: <nickname> left`

---

## 3. 构建

在项目目录执行：

```bash
g++ -std=c++17 -Wall -Wextra main.cpp Server.cpp Connection.cpp EventLoop.cpp Channel.cpp -o smallchat-epoll
g++ -std=c++17 -Wall -Wextra client.cpp -o smallchat-client
```

---

## 4. 运行

1. 启动服务端：

```bash
./smallchat-epoll
```

2. 启动一个或多个客户端（新开终端）：

```bash
./smallchat-client
```

也支持指定地址端口：

```bash
./smallchat-client 127.0.0.1 8888
```

---

## 5. 使用流程

客户端连接后会看到：

- 先输入昵称（第一行）
- 再输入聊天内容
- 输入 `/quit` 退出

说明：

- 普通消息只发给其他客户端，不会回显给发送者自己。
- 如果只有一个客户端在线，发送聊天内容时看不到广播是正常行为。

---

## 6. 核心实现说明

### 6.1 非阻塞

`Server.cpp` 中通过 `setNonBlocking(int fd)` 设置：

- `listenFd_` 非阻塞
- 每个 `clientFd` 非阻塞

### 6.2 ET 读空原则

在 ET 模式下，回调中必须持续读取直到 `EAGAIN/EWOULDBLOCK`，否则可能丢事件。

当前实现中：

- 新连接：`handleNewConnection()` 内 `while(accept)` 到 `EAGAIN`
- 读数据：`handleClientMessage()` 内 `while(read)` 到 `EAGAIN`

### 6.3 按行组包

客户端可能一次发来多行，或一行被拆包。服务端为每个连接维护 `inputBuffer`：

- 新数据 append 到 buffer
- 循环查找 `\n`
- 切出完整一行再执行业务逻辑

---

## 7. 常见问题

### Q1: 为什么发送后自己看不到？

因为当前是“广播给其他客户端”。如果要“自己也看到自己消息”，需要改成广播给所有人（含自己）。

### Q2: 启动时报 `bind failed`？

通常是端口 `8888` 被占用。结束旧进程后重试，或者改端口。

### Q3: 为什么 ET 下必须循环读？

ET 只在状态变化时通知一次；如果不把内核缓冲区读空，后续可能不再收到可读事件。

---

## 8. 后续可扩展方向

- 昵称重复检测
- 私聊命令（如 `/msg <user> <text>`）
- 在线用户列表（如 `/who`）
- 心跳与超时踢线
- 写缓冲与高水位控制
- 多 Reactor / 线程池

