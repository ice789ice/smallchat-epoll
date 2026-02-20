# smallchat-epoll

A terminal chatroom demo built on `epoll`, currently using:

- Reactor architecture
- ET (Edge Trigger) mode
- Non-blocking sockets
- Single-threaded event loop

Features:

- Multi-client connections
- First input line is used as nickname registration
- Following lines are broadcast to other online clients
- `/quit` for graceful exit

---

## 1. Project Structure

```text
main.cpp
Server.h / Server.cpp
EventLoop.h / EventLoop.cpp
Channel.h / Channel.cpp
Connection.h / Connection.cpp
client.cpp
```

Responsibilities:

- `EventLoop`: wraps `epoll_wait` main loop and `epoll_ctl` add/remove logic.
- `Channel`: wraps one fd, its events (`EPOLLIN | EPOLLET`), and `readCallback`.
- `Server`: owns listen socket, accepts new clients, manages client state, and applies chat protocol.
- `Connection`: per-connection read/write wrapper (read chunk / send message / closed state).
- `client.cpp`: terminal client, uses `select` to watch both stdin and socket.

---

## 2. Chat Protocol

Server processes input **line by line** (split by `\n`):

1. First line: nickname
2. From second line: normal chat messages (broadcast to other clients)
3. `/quit`: broadcast leave message and disconnect

Example:

```text
alice
hello everyone
/quit
```

Broadcast formats:

- Join: `[system]: <nickname> joined`
- Chat: `[<nickname>]: <message>`
- Leave: `[system]: <nickname> left`

---

## 3. Build

Run in the project directory:

```bash
g++ -std=c++17 -Wall -Wextra main.cpp Server.cpp Connection.cpp EventLoop.cpp Channel.cpp -o smallchat-epoll
g++ -std=c++17 -Wall -Wextra client.cpp -o smallchat-client
```

---

## 4. Run

1. Start server:

```bash
./smallchat-epoll
```

2. Start one or multiple clients (new terminals):

```bash
./smallchat-client
```

You can also pass host/port:

```bash
./smallchat-client 127.0.0.1 8888
```

---

## 5. Usage Flow

After client connects:

- Enter nickname first (first line)
- Enter chat messages after that
- Enter `/quit` to leave

Notes:

- Normal messages are sent to **other** clients only (not echoed to sender).
- If only one client is online, no broadcast output is expected.

---

## 6. Core Implementation Notes

### 6.1 Non-blocking mode

`Server.cpp` uses `setNonBlocking(int fd)` for:

- `listenFd_`
- every `clientFd`

### 6.2 ET drain-until-EAGAIN rule

In ET mode, callbacks must keep reading/accepting until `EAGAIN`/`EWOULDBLOCK`, otherwise events may be missed.

Current behavior:

- New connections: `handleNewConnection()` loops `accept(...)` until `EAGAIN`
- Read messages: `handleClientMessage()` loops `read(...)` until `EAGAIN`

### 6.3 Line framing

A recv chunk can contain multiple lines, or one line may be fragmented.  
Server keeps an `inputBuffer` per client:

- append new chunk
- find `\n` in a loop
- extract a complete line and process protocol logic

---

## 7. FAQ

### Q1: Why can't I see my own sent message?

Because current logic broadcasts to other clients only.  
If needed, it can be changed to broadcast to everyone (including sender).

### Q2: Why `bind failed` on startup?

Most likely port `8888` is already in use. Stop the previous process or switch port.

### Q3: Why must ET mode loop-read?

ET notifies on state transitions. If socket buffer is not drained, you may not receive another readable event.

---

## 8. Possible Extensions

- Duplicate nickname check
- Private message command (e.g. `/msg <user> <text>`)
- Online user list (e.g. `/who`)
- Heartbeat and idle timeout kick
- Output buffer / high-water mark handling
- Multi-reactor / thread pool

