# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ibridger is an IPC messaging and RPC framework that bridges processes across language boundaries. It uses a client-server architecture with Protocol Buffers for serialization, Unix Domain Sockets (macOS/Linux) and Named Pipes (Windows) for transport.

## Architecture

Four-layer stack (bottom-up):

1. **Transport** (`core/include/ibridger/transport/`) — Abstract `ITransport`/`IConnection` interfaces with platform-specific implementations (Unix socket, Named pipe)
2. **Protocol** (`core/include/ibridger/protocol/`) — Length-prefixed framing `[4-byte BE length][payload]` + protobuf `Envelope` codec
3. **RPC** (`core/include/ibridger/rpc/`) — Service registry, method dispatch, request-ID correlation, `Server` and `Client` engines
4. **SDK** (`sdk/cpp/`, `sdk/js/`) — Language-specific ergonomic public APIs

**Key principle:** Each language SDK implements the wire protocol natively (sockets + protobuf). No C++ native bindings needed. The JS SDK is pure TypeScript using Node.js `net` module.

## Build Commands

### C++ (CMake)
```bash
cmake -B build -DIBRIDGER_BUILD_TESTS=ON -DIBRIDGER_BUILD_EXAMPLES=ON
cmake --build build
cd build && ctest                         # run all C++ tests (must run from build dir)
cd build && ctest -R <test_name>          # run a single test
```

### JavaScript SDK
```bash
cd sdk/js
npm install
npm test                                  # run all JS tests
npx jest --testPathPattern=<pattern>      # run specific test
npm run build                             # compile TypeScript
npm run generate-proto                    # regenerate protobuf types
```

### Integration Tests
```bash
cd tests/integration
npm test                                  # runs C++ server + JS client tests
```

## Proto Definitions

All `.proto` files live in `proto/ibridger/`. The `Envelope` message in `envelope.proto` is the wire protocol envelope — all SDK implementations must speak this format. Changes to `envelope.proto` affect every language SDK.

## Implementation Roadmap

See `docs/ROADMAP.md` for the 25-phase implementation plan. Each phase is a single focused task with defined inputs, outputs, and tests.

## Conventions

- C++17, CMake 3.20+, Google Test for C++ tests
- TypeScript 5+, Jest for JS tests, protobufjs for JS protobuf
- Every new class/function requires unit tests
- Transport implementations are guarded by platform preprocessor checks (`__unix__`, `__APPLE__`, `_WIN32`)
- Error handling uses `std::error_code` (C++) and thrown errors with status codes (JS)
