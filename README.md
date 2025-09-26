### My Mini Server

A tiny multi-client TCP chat server implemented in C using `select(2)`. It listens on `127.0.0.1:<port>`, accepts multiple clients, and broadcasts newline-delimited messages to all other connected clients with a `client <id>:` prefix. Clean shutdown is supported via `Ctrl+C` (SIGINT) or `Ctrl+D` on stdin.

## Features
- **Multiple clients**: Single-threaded, multiplexed with `select(2)`.
- **Broadcast chat**: Each complete line from a client is sent to others as `client N: <msg>`.
- **Join/leave notices**: Sends `server: client N just arrived/left`.
- **Graceful shutdown**: `Ctrl+C` (SIGINT) or `Ctrl+D` on server stdin.
- **Robust buffering**: Handles partial reads and aggregates until a `\n` is received.

## Requirements
- Linux or any POSIX-like system
- CC Compiler

## Clone
```bash
git clone https://github.com/sbibers/my_mini_server.git
```

## Build
```bash
cc -Wall -Wextra -Werror my_mini_server.c -o my_mini_server
```

## Run
```bash
./my_mini_server <port>
```

- The server binds to `127.0.0.1` only (localhost).
- `<port>` must be provided (e.g., `8080`).

## Connect (from clients)
Use `nc` (netcat) or `telnet` from the same machine:
```bash
# Terminal 1: start server
./my_mini_server 8080

# Terminal 2: client A
nc 127.0.0.1 8080

# Terminal 3: client B
nc 127.0.0.1 8080
```

Type a message in client A, press Enter — client B will receive a line like:
```
client 0: hello there
```

When a client connects/disconnects, other clients will see lines such as:
```
server: client 1 just arrived
server: client 1 just left
```

## Protocol and behavior
- **Delimiter**: Lines are terminated by `\n`. Data is buffered until a newline is seen.
- **Prefixing**: Outgoing broadcast lines are prefixed `client <id>:`; the sender does not receive its own echo.
- **Address**: IPv4 only; bound to `127.0.0.1`.
- **Backlog**: Uses `SOMAXCONN` for `listen(2)` backlog.

## Shutdown
- Press `Ctrl+C` in the server terminal (SIGINT), or
- Press `Ctrl+D` (EOF) in the server terminal. The server prints `Server shutting down ...` to stderr and exits.

## Troubleshooting
- "Wrong number of arguments" → run as `./my_mini_server <port>`.
- "Fatal error" → indicates a system call or allocation failure (e.g., port in use, insufficient resources). Try another port or ensure you have permissions.
- Can’t connect → ensure you are connecting to `127.0.0.1 <port>` from the same machine.

## Code map
- `my_mini_server.c` — all logic: socket setup, client list, buffering, broadcasting, and shutdown handling.

## Notes
- This is a minimal educational server: no authentication, no IPv6, no TLS, no message size limits beyond process memory.
- Client IDs are incremental per process lifetime and start at 0.

