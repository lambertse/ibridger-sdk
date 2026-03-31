# iBridger

IPC messaging and RPC framework that bridges processes across language boundaries. Uses Unix Domain Sockets (macOS/Linux) or Named Pipes (Windows) for transport and Protocol Buffers for serialization. Any language that can open a socket and encode protobuf can implement an SDK — no C++ bindings required.

## Architecture

Four-layer stack (bottom-up):

| Layer | Responsibility | C++ | JavaScript/TypeScript |
|-------|---------------|-----|-----------------------|
| Transport | Platform IPC | `UnixSocketTransport` | `UnixSocketConnection` |
| Protocol | Framing + serialization | `FramedConnection`, `EnvelopeCodec` | `FramedConnection`, `EnvelopeCodec` |
| RPC | Dispatch + correlation | `Server`, `Client` | `IBridgerServer`, `IBridgerClient` |
| SDK | Ergonomic public API | `ServerBuilder`, `ClientStub` | `IBridgerServer`, `IBridgerClient` |

```
┌──────────────────────┐  ┌──────────────────────┐  ┌─────────────────┐
│   C++ SDK            │  │   JS/TS SDK           │  │   Future SDK    │
│   sdk/cpp/           │  │   sdk/js/             │  │   (Go, Python…) │
└──────────┬───────────┘  └──────────┬────────────┘  └────────┬────────┘
           │                         │                         │
           │      Protobuf Envelope over length-framed IPC     │
           ▼                         ▼                         ▼
┌──────────────────────────────────────────────────────────────────────┐
│         Unix Domain Socket  /  Named Pipe (Windows)                  │
└──────────────────────────────────────────────────────────────────────┘
```

## Quick Start

### C++ Server + JS Client

```bash
# Build C++ (once)
cmake -B build -DIBRIDGER_BUILD_TESTS=ON -DIBRIDGER_BUILD_EXAMPLES=ON
cmake --build build

# Terminal 1 — start C++ echo server
./build/sdk/cpp/echo_server /tmp/ibridger.sock

# Terminal 2 — JS client
cd sdk/js && npm install
npx ts-node examples/echo-client.ts /tmp/ibridger.sock
```

### Pure JS (Server + Client)

```bash
cd sdk/js && npm install

# Terminal 1
npx ts-node examples/echo-server.ts /tmp/ibridger.sock

# Terminal 2
npx ts-node examples/echo-client.ts /tmp/ibridger.sock
```

### Ping Health Check

```bash
npx ts-node examples/ping-client.ts /tmp/ibridger.sock
```

## Building

### C++ (CMake)

```bash
cmake -B build -DIBRIDGER_BUILD_TESTS=ON -DIBRIDGER_BUILD_EXAMPLES=ON
cmake --build build
cd build && ctest                       # all tests
cd build && ctest -R <TestName>         # single test
cd build && ctest --output-on-failure   # verbose on failure
```

Requires: CMake 3.20+, C++17 compiler, internet access (FetchContent downloads protobuf + googletest).

### JavaScript SDK

```bash
cd sdk/js
npm install
npm test                                # all JS tests
npx jest --testPathPattern=<pattern>    # specific test file
npm run build                           # compile TypeScript → dist/
npm run generate-proto                  # regenerate protobuf bindings
```

Requires: Node.js 18+.

### Integration Tests (C++ server + JS client)

```bash
cd tests/integration
npm install
npm test
```

Requires: C++ build to be complete (for `build/sdk/cpp/echo_server`).

## Project Layout

```
proto/ibridger/         Wire protocol .proto definitions
core/                   C++ transport/protocol/RPC core (ibridger::core)
  include/ibridger/     Public headers
  src/                  Implementations
  tests/                Unit tests (Google Test)
sdk/cpp/                C++ SDK — ergonomic wrapper around core
sdk/js/                 TypeScript SDK — pure Node.js, no native bindings
  src/                  Library source
  tests/                Jest unit tests
  examples/             Runnable TypeScript examples
tests/integration/      Cross-language integration tests
docs/                   Guides and specifications
```

## Wire Protocol

All SDKs speak the same wire format defined in `docs/WIRE_PROTOCOL.md`:

- **Framing:** `[4-byte big-endian length][payload]`, max 16 MB per frame
- **Envelope:** Protocol Buffers message (`proto/ibridger/envelope.proto`)
- **Built-in:** `ibridger.Ping` service for health checks on every server

## Documentation

| Document | Description |
|----------|-------------|
| `docs/ROADMAP.md` | 25-phase implementation plan with dependency graph |
| `docs/WIRE_PROTOCOL.md` | Complete wire protocol specification |
| `docs/adding-a-language.md` | Guide for implementing a new language SDK |
| `CLAUDE.md` | Guidance for Claude Code sessions in this repo |
