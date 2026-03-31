# JS SDK Examples

Three runnable scripts demonstrating the ibridger JS/TypeScript SDK using a
**pure JS server** — no C++ process required.

## Prerequisites

Install JS dependencies (from `sdk/js/`):
```bash
npm install
```

## echo-server.ts

Starts a JS ibridger server exposing two services:
- `EchoService/Echo` — uppercases the message and stamps a timestamp
- `ibridger.Ping/Ping` — built-in health check (auto-registered)

```bash
npx ts-node sdk/js/examples/echo-server.ts [socket-path]
# default: /tmp/ibridger_echo.sock
```

## ping-client.ts

Connects to the server, sends a Ping, and prints the server ID and
round-trip latency.

```bash
# Terminal 1
npx ts-node sdk/js/examples/echo-server.ts

# Terminal 2
npx ts-node sdk/js/examples/ping-client.ts
```

Expected output:
```
Connecting to /tmp/ibridger_echo.sock ...
Connected.
Pong from server_id : ibridger-server
Server timestamp    : 1711900000123 ms
Round-trip latency  : 3 ms
```

## echo-client.ts

Connects to the server, sends an `EchoRequest`, and prints the uppercased
response.

```bash
# Terminal 1
npx ts-node sdk/js/examples/echo-server.ts

# Terminal 2
npx ts-node sdk/js/examples/echo-client.ts /tmp/ibridger_echo.sock "hello from JS"
```

Expected output (client):
```
Connecting to /tmp/ibridger_echo.sock ...
Connected.
Sending : "hello from JS"
Received: "HELLO FROM JS"
Server timestamp  : 1711900000456 ms
Round-trip latency: 4 ms
```

Expected output (server):
```
  Echo  <- "hello from JS"
  Echo  -> "HELLO FROM JS"
```

## Against the C++ server

The client examples also work against the C++ echo server (Phase 17) since
both sides speak the same wire protocol:

```bash
# Build C++ server first (from repo root)
cmake -B build -DIBRIDGER_BUILD_EXAMPLES=ON && cmake --build build

# Terminal 1 — C++ server
./build/sdk/cpp/echo_server /tmp/ibridger_echo.sock

# Terminal 2 — JS clients connect seamlessly
npx ts-node sdk/js/examples/ping-client.ts
npx ts-node sdk/js/examples/echo-client.ts /tmp/ibridger_echo.sock "cross-language!"
```
