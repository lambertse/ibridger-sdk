# Adding a New Language SDK

ibridger's wire protocol is fully self-contained and can be implemented independently in any language that supports Unix domain sockets (or TCP) and Protocol Buffers. This guide walks through the four components you need to write, with Go as a worked example.

## What to Implement

### 1. Transport — raw connection

Open a Unix domain socket and wrap it in a type with two low-level operations:

- `send(data: bytes)` — write all bytes, looping on partial writes
- `recv(n: int) → bytes` — read exactly n bytes, looping on partial reads

```go
// Go example
conn, err := net.Dial("unix", socketPath)
```

On Windows, `net.Dial("unix", path)` also works for named pipes via the named-pipe path convention, or use `npipe` package for Windows named pipes.

### 2. Protocol — length-prefix framing

Wrap the connection so callers work with whole messages, not byte streams:

- `send_frame(payload: bytes)` — write `[4-byte big-endian length][payload]`
- `recv_frame() → bytes` — read 4-byte length header, then read exactly that many bytes

**Constraints (from `docs/WIRE_PROTOCOL.md`):**
- Maximum frame size: 16,777,216 bytes (16 MiB). Reject larger frames with an error.
- Zero-length frames are valid (reserved for future heartbeat use).

### 3. Protocol — Envelope codec

Wrap the framed connection with protobuf serialization. The `Envelope` message is defined in `proto/ibridger/envelope.proto`. Generate language bindings with `protoc`.

- `send(envelope: Envelope)` — `proto.Marshal(envelope)` → `send_frame(bytes)`
- `recv() → Envelope` — `recv_frame()` → `proto.Unmarshal(bytes, &env)`

### 4. RPC client

Manage a monotonically increasing `request_id` (start at 1, increment per call):

1. Build `Envelope{type: REQUEST, request_id: N, service_name, method_name, payload}`
2. `codec.send(envelope)`
3. `response = codec.recv()`
4. Assert `response.request_id == N` — mismatch indicates a protocol bug
5. If `response.status != OK`, return the status as an error
6. Return `response.payload` for the caller to deserialize

## Protobuf Generation

```bash
# From the repo root
protoc \
  --<lang>_out=sdk/<lang>/generated \
  -I proto \
  proto/ibridger/envelope.proto \
  proto/ibridger/rpc.proto \
  proto/ibridger/constants.proto
```

The `WireConstant` enum in `constants.proto` gives you `MAX_FRAME_SIZE` and `DEFAULT_TIMEOUT_MS` as proto-sourced constants so you don't hard-code them.

## RPC Server (Optional)

If you also want to run a server in your language:

1. `net.Listen("unix", socketPath)` — accept connections in a loop
2. Per connection: loop `codec.recv()` → dispatch → `codec.send(response)`
3. Dispatch: look up `service_name` + `method_name`, call handler, build response `Envelope`
4. Return `NOT_FOUND` status if service or method is unknown
5. Auto-register `ibridger.Ping` / `Ping` so health checks work

## Test Matrix

Before shipping, verify these scenarios (run against both C++ and your own server):

| # | Scenario | What to check |
|---|----------|---------------|
| 1 | Ping roundtrip | `Pong.server_id` is non-empty, `timestamp_ms` is recent |
| 2 | Echo call | Response payload matches request (uppercase for ibridger's EchoService) |
| 3 | NOT_FOUND | Call a nonexistent service, get `status=NOT_FOUND`, non-empty `error_message` |
| 4 | Sequential calls | `request_id` increments 1, 2, 3… across calls |
| 5 | Reconnect | Disconnect, reconnect, call succeeds — IDs reset to 1 |
| 6 | Concurrent clients | Multiple connections simultaneously, no cross-contamination |

## Go SDK Outline

```
sdk/go/
├── go.mod
├── transport/
│   └── unix_socket.go         # net.Dial("unix", …), send/recv loops
├── protocol/
│   ├── framing.go             # 4-byte length-prefix read/write
│   └── codec.go               # proto.Marshal / proto.Unmarshal + framing
├── rpc/
│   ├── client.go              # IBridgerClient with Call() and Ping()
│   └── server.go              # IBridgerServer with Register() and Listen()
├── generated/
│   ├── envelope.pb.go         # protoc output
│   └── rpc.pb.go
└── examples/
    ├── echo_server/main.go
    └── echo_client/main.go
```

```go
// rpc/client.go sketch
type IBridgerClient struct {
    endpoint  string
    conn      net.Conn
    codec     *protocol.Codec
    requestID uint64
    mu        sync.Mutex
}

func (c *IBridgerClient) Call(service, method string, payload []byte) ([]byte, error) {
    c.mu.Lock()
    defer c.mu.Unlock()
    c.requestID++
    env := &ibridger.Envelope{
        Type: ibridger.MessageType_REQUEST,
        RequestId: c.requestID,
        ServiceName: service,
        MethodName: method,
        Payload: payload,
    }
    if err := c.codec.Send(env); err != nil {
        return nil, err
    }
    resp, err := c.codec.Recv()
    if err != nil {
        return nil, err
    }
    if resp.RequestId != c.requestID {
        return nil, fmt.Errorf("request_id mismatch: got %d, want %d", resp.RequestId, c.requestID)
    }
    if resp.Status != ibridger.StatusCode_OK {
        return nil, fmt.Errorf("rpc error %s: %s", resp.Status, resp.ErrorMessage)
    }
    return resp.Payload, nil
}
```

## Wire Protocol Reference

See `docs/WIRE_PROTOCOL.md` for exact framing format, all Envelope field semantics, status code definitions, request ID rules, and the Ping service contract.
