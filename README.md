# iBridger

[![CI](https://github.com/lambertse/ibridger/actions/workflows/ci.yml/badge.svg)](https://github.com/lambertse/ibridger/actions/workflows/ci.yml)
[![npm version](https://img.shields.io/npm/v/@lambertse/ibridger.svg)](https://www.npmjs.com/package/@lambertse/ibridger)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

IPC/RPC framework that bridges processes across language boundaries. A C++ server and a JavaScript client talk to each other — or any future language — using the same wire protocol: **Unix domain sockets + Protocol Buffers**, no shared memory, no native bindings.

```
┌──────────────────┐  ┌──────────────────┐  ┌─────────────────┐
│   C++ SDK        │  │   JS/TS SDK      │  │  Go, Python, …  │
│   sdk/cpp/       │  │   sdk/js/        │  │  (your SDK)     │
└────────┬─────────┘  └────────┬─────────┘  └───────┬─────────┘
         │                     │                     │
         │       Protobuf Envelope over framed IPC   │
         ▼                     ▼                     ▼
┌──────────────────────────────────────────────────────────────┐
│           Unix Domain Socket  /  Named Pipe (Windows)        │
└──────────────────────────────────────────────────────────────┘
```

## Quick Start

### JavaScript / TypeScript server + client

```bash
npm install @lambertse/ibridger
```

```typescript
// server.ts
import { IBridgerServer, typedMethod } from '@lambertse/ibridger';
import { ibridger } from '@lambertse/ibridger';

const server = new IBridgerServer({ endpoint: '/tmp/my.sock' });
server.register('EchoService', {
  Echo: typedMethod(
    ibridger.examples.EchoRequest,
    ibridger.examples.EchoResponse,
    async (req) => ({ message: req.message.toUpperCase() }),
  ),
});
await server.start();
```

```typescript
// client.ts
import { IBridgerClient } from '@lambertse/ibridger';
import { ibridger } from '@lambertse/ibridger';

const client = new IBridgerClient({ endpoint: '/tmp/my.sock' });
await client.connect();
const resp = await client.call(
  'EchoService', 'Echo',
  { message: 'hello' },
  ibridger.examples.EchoRequest,
  ibridger.examples.EchoResponse,
);
console.log(resp.message); // "HELLO"
client.disconnect();
```

### Pure C++ server + client

**Server** — define a service by subclassing `ServiceBase`, then wire it up with `ServerBuilder`:

```cpp
// my_service.h
#include "ibridger/sdk/service_base.h"
#include "my_service.pb.h"   // your protobuf-generated types

class GreetService : public ibridger::sdk::ServiceBase {
public:
    GreetService() : ServiceBase("GreetService") {
        register_method<GreetRequest, GreetResponse>(
            "Hello",
            [](const GreetRequest& req) {
                GreetResponse resp;
                resp.set_message("Hello, " + req.name() + "!");
                return resp;
            });
    }
};
```

```cpp
// server_main.cpp
#include "ibridger/sdk/server_builder.h"
#include "my_service.h"

int main() {
    auto server = ibridger::sdk::ServerBuilder()
        .set_endpoint("/tmp/my.sock")
        .add_service(std::make_shared<GreetService>())
        .build();

    server->start();
    ::pause();   // wait for SIGINT
    server->stop();
}
```

**Client** — connect with `ClientStub` and call with typed proto messages:

```cpp
#include "ibridger/sdk/client_stub.h"
#include "my_service.pb.h"

int main() {
    ibridger::rpc::ClientConfig cfg;
    cfg.endpoint = "/tmp/my.sock";
    ibridger::sdk::ClientStub stub(cfg);

    stub.connect();

    GreetRequest req;
    req.set_name("world");

    auto [resp, err] = stub.call<GreetRequest, GreetResponse>(
        "GreetService", "Hello", req);

    if (!err) std::cout << resp.message() << "\n";  // "Hello, world!"
}
```

**CMakeLists.txt** for a consumer project:

```cmake
find_package(ibridger CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE ibridger::sdk::cpp)
```

### C++ server + JavaScript client

```bash
# Build C++
cmake -B build -DIBRIDGER_BUILD_EXAMPLES=ON && cmake --build build

# Terminal 1 — C++ echo server
./build/sdk/cpp/echo_server /tmp/ibridger.sock

# Terminal 2 — JS client (from sdk/js/)
npx ts-node examples/echo-client.ts /tmp/ibridger.sock
```

### Health check (built-in Ping, any server)

```typescript
const pong = await client.ping();
console.log(pong.serverId, Number(pong.timestampMs));
```

## Architecture

Four-layer stack (bottom-up):

| Layer | Responsibility | C++ | JavaScript/TypeScript |
|---|---|---|---|
| **Transport** | Platform IPC | `UnixSocketTransport` | `UnixSocketConnection` |
| **Protocol** | Framing + serialization | `FramedConnection`, `EnvelopeCodec` | `FramedConnection`, `EnvelopeCodec` |
| **RPC** | Dispatch + correlation | `Server`, `Client` | `IBridgerServer`, `IBridgerClient` |
| **SDK** | Ergonomic public API | `ServerBuilder`, `ClientStub` | same as RPC layer |

Wire format: `[4-byte big-endian length][protobuf Envelope]`, max 16 MB per frame. Documented in [`docs/WIRE_PROTOCOL.md`](docs/WIRE_PROTOCOL.md).

## Building from source

### C++ (CMake 3.20+, C++17)

```bash
cmake -B build -DIBRIDGER_BUILD_TESTS=ON -DIBRIDGER_BUILD_EXAMPLES=ON
cmake --build build
cd build && ctest --output-on-failure       # all C++ tests
cd build && ctest -R <TestName>             # single test
```

On macOS, `brew install protobuf` is recommended so CMake uses the system library instead of compiling protobuf from source.

### JavaScript SDK (Node.js 18+)

```bash
cd sdk/js
npm install
npm test                                    # all JS tests
npx jest --testPathPattern=<pattern>        # specific test
npm run build                               # compile TypeScript → dist/
```

### Integration tests (C++ server ↔ JS client)

```bash
cd tests/integration && npm install && npm test
```

Requires the C++ build to be complete (`build/sdk/cpp/echo_server`).

## Project layout

```
proto/ibridger/         Wire protocol .proto definitions
core/                   C++ transport / protocol / RPC core (ibridger::core)
sdk/cpp/                C++ SDK wrapper — ServerBuilder, ClientStub
sdk/js/                 TypeScript SDK — pure Node.js, no native bindings
tests/integration/      Cross-language integration tests (C++ ↔ JS)
docs/                   Wire protocol spec, roadmap, adding-a-language guide
```

## Implementing a new language SDK

Any language that can open a Unix domain socket and encode/decode protobuf messages can implement an SDK — the C++ core is not required. See [`docs/adding-a-language.md`](docs/adding-a-language.md) for the four-component checklist and a Go SDK outline.

## Documentation

| Document | Description |
|---|---|
| [`docs/WIRE_PROTOCOL.md`](docs/WIRE_PROTOCOL.md) | Authoritative wire format spec |
| [`docs/adding-a-language.md`](docs/adding-a-language.md) | Guide for new language SDKs |
| [`docs/ROADMAP.md`](docs/ROADMAP.md) | 25-phase implementation plan |
| [npm package README](sdk/js/README.md) | JS/TS SDK API reference |

## License

MIT
