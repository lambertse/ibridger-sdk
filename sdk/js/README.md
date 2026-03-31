# @lambertse/ibridger

[![npm version](https://img.shields.io/npm/v/@lambertse/ibridger.svg)](https://www.npmjs.com/package/@lambertse/ibridger)
[![CI](https://github.com/lambertse/ibridger/actions/workflows/ci.yml/badge.svg)](https://github.com/lambertse/ibridger/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://github.com/lambertse/ibridger/blob/main/LICENSE)

TypeScript/Node.js SDK for [iBridger](https://github.com/lambertse/ibridger) — an IPC/RPC framework that bridges processes across language boundaries using Unix domain sockets and Protocol Buffers.

> **Cross-language by design.** A JS server talks to a C++ client (or Go, Python, …) using the same wire protocol — no native bindings, no shared memory, just sockets + protobuf.

## Installation

```bash
npm install @lambertse/ibridger
```

Requires **Node.js 18+**.

## Quick Start

### Server

```typescript
import { IBridgerServer, typedMethod } from '@lambertse/ibridger';
import { ibridger } from '@lambertse/ibridger';

const server = new IBridgerServer({ endpoint: '/tmp/my-service.sock' });

// Register a typed RPC method — proto encode/decode is handled automatically.
server.register('GreetService', {
  Hello: typedMethod(
    ibridger.examples.EchoRequest,
    ibridger.examples.EchoResponse,
    async (req) => ({ message: `Hello, ${req.message}!` }),
  ),
});

await server.start();
console.log('Listening on /tmp/my-service.sock');
// Built-in ibridger.Ping/Ping health check is registered automatically.
```

### Client

```typescript
import { IBridgerClient, RpcError } from '@lambertse/ibridger';
import { ibridger } from '@lambertse/ibridger';

const client = new IBridgerClient({ endpoint: '/tmp/my-service.sock' });
await client.connect();

const response = await client.call(
  'GreetService',
  'Hello',
  { message: 'world' },
  ibridger.examples.EchoRequest,
  ibridger.examples.EchoResponse,
);
console.log(response.message); // "Hello, world!"

client.disconnect();
```

### Health check (built-in Ping)

Every iBridger server exposes a built-in `ibridger.Ping/Ping` method — no registration needed.

```typescript
const pong = await client.ping();
console.log(pong.serverId);     // "ibridger-server"
console.log(Number(pong.timestampMs)); // server epoch ms
```

## API

### `IBridgerServer`

```typescript
new IBridgerServer(config: ServerConfig)
```

| Config field | Type | Default | Description |
|---|---|---|---|
| `endpoint` | `string` | — | Unix socket path (e.g. `/tmp/my.sock`) |
| `registerBuiltins` | `boolean` | `true` | Auto-register built-in Ping service |

| Method | Description |
|---|---|
| `register(service, methods)` | Register service methods. Calling multiple times with the same service merges methods. Returns `this` for chaining. |
| `start()` | Bind and begin accepting connections. |
| `stop()` | Close all connections and stop listening. |
| `isRunning` | `boolean` getter. |

### `IBridgerClient`

```typescript
new IBridgerClient(config: { endpoint: string; defaultTimeout?: number })
```

| Method | Description |
|---|---|
| `connect()` | Open connection to the server. |
| `disconnect()` | Close the connection. |
| `call(service, method, request, ReqType, RespType, options?)` | Make a typed RPC call. Throws `RpcError` on non-OK status, `TimeoutError` on timeout. |
| `ping(options?)` | Convenience shorthand for the built-in Ping health check. |
| `isConnected` | `boolean` getter. |

**`CallOptions`**

```typescript
interface CallOptions {
  timeout?: number;           // ms, defaults to 30 000
  metadata?: Record<string, string>;
}
```

### `typedMethod`

Wraps a typed async handler into the raw `MethodHandler` format, handling proto encode/decode automatically.

```typescript
typedMethod<TReq, TResp>(
  reqType: ProtoType<TReq>,
  respType: ProtoType<TResp>,
  handler: (req: TReq) => Promise<TResp>,
): MethodHandler
```

### Error types

```typescript
// Thrown when the server returns a non-OK status code.
class RpcError extends Error {
  readonly status: ibridger.StatusCode;
}

// Thrown when options.timeout ms elapse before a reply arrives.
class TimeoutError extends Error {}
```

## Error handling

```typescript
import { RpcError, TimeoutError, IBridgerClient } from '@lambertse/ibridger';
import { ibridger } from '@lambertse/ibridger';

try {
  await client.call('MyService', 'DoWork', req, ReqType, RespType, { timeout: 5000 });
} catch (e) {
  if (e instanceof TimeoutError) {
    console.error('Call timed out');
  } else if (e instanceof RpcError) {
    console.error(`RPC error ${e.status}: ${e.message}`);
    if (e.status === ibridger.StatusCode.NOT_FOUND) { /* ... */ }
  }
}
```

## Cross-language interoperability

This SDK speaks the same wire protocol as the C++ SDK (`ibridger::sdk::cpp`). Mix and match freely:

| Server | Client | Works? |
|---|---|---|
| JS (`IBridgerServer`) | JS (`IBridgerClient`) | ✓ |
| C++ (`ServerBuilder`) | JS (`IBridgerClient`) | ✓ |
| JS (`IBridgerServer`) | C++ (`ClientStub`) | ✓ |
| C++ (`ServerBuilder`) | C++ (`ClientStub`) | ✓ |

The wire format: `[4-byte big-endian length][protobuf Envelope payload]`. See [WIRE_PROTOCOL.md](https://github.com/lambertse/ibridger/blob/main/docs/WIRE_PROTOCOL.md) for the full spec.

## Proto definitions

The `ibridger` namespace re-exports all generated types from the bundled `.proto` files. For custom service messages, generate your own types with [protobufjs-cli](https://github.com/protobufjs/protobuf.js/tree/master/cli) and pass the resulting classes to `typedMethod` / `client.call`.

## Limitations

- **One outstanding request per client** — `call()` throws if called while another is in flight.
- **Unix domain sockets only** — Windows Named Pipe support is planned but not yet implemented.
- **No streaming** — request-response only.

## More

- [GitHub repository & C++ SDK](https://github.com/lambertse/ibridger)
- [Wire Protocol Specification](https://github.com/lambertse/ibridger/blob/main/docs/WIRE_PROTOCOL.md)
- [Adding a language SDK](https://github.com/lambertse/ibridger/blob/main/docs/adding-a-language.md)
- [Changelog / Releases](https://github.com/lambertse/ibridger/releases)

## License

MIT
