# ibridger Wire Protocol Specification

This document is the authoritative behavioural contract for all ibridger SDK
implementations. Any SDK that deviates from this spec will fail the
cross-language integration tests (Phase 22).

Numeric constants are defined in `proto/ibridger/constants.proto` and
generated into every language automatically. String constants are listed here
and enforced by integration tests.

---

## 1. Transport

| Property | Value |
|---|---|
| Socket type | Unix Domain Socket (`AF_UNIX`, `SOCK_STREAM`) on macOS/Linux |
| Windows | Named Pipe (future — stub only) |
| Byte order | Big-endian for all multi-byte integers |

---

## 2. Framing

Every message is length-prefixed:

```
┌─────────────────────────┬──────────────────────────────┐
│  4-byte BE uint32       │  payload bytes               │
│  (payload length)       │  (protobuf-serialized        │
│                         │   ibridger.Envelope)         │
└─────────────────────────┴──────────────────────────────┘
```

### Rules

| Rule | Value / Behaviour |
|---|---|
| Max frame payload | `MAX_FRAME_SIZE = 16777216` bytes (16 MiB) — from `constants.proto` |
| Zero-length frame | Valid — used as a no-op / keepalive. Receiver returns empty payload, no error. |
| Oversized send | MUST be rejected before writing to the socket |
| Oversized recv | MUST be rejected after reading the 4-byte header; close the connection |
| Partial read | MUST loop until exactly N bytes are received (or error on EOF) |
| Premature EOF | Treat as `connection_reset` / `ECONNRESET` |

---

## 3. Envelope

The payload of every frame is a serialized `ibridger.Envelope` protobuf
(defined in `proto/ibridger/envelope.proto`).

### Request (client → server)

| Field | Required | Notes |
|---|---|---|
| `type` | Yes | Must be `REQUEST` |
| `request_id` | Yes | Monotonically increasing uint64, starting at **1** |
| `service_name` | Yes | e.g. `"EchoService"` |
| `method_name` | Yes | e.g. `"Echo"` |
| `payload` | No | Serialized request proto |
| `metadata` | No | Arbitrary string key/value pairs |

### Response (server → client)

| Field | Required | Notes |
|---|---|---|
| `type` | Yes | `RESPONSE` on success, `ERROR` on failure |
| `request_id` | Yes | MUST mirror the request's `request_id` exactly |
| `status` | Yes | `OK` on success; `NOT_FOUND`, `INTERNAL`, etc. on error |
| `payload` | Success only | Serialized response proto |
| `error_message` | Error only | Human-readable error description |

### Correlation

The client MUST validate that `response.request_id == request.request_id`.
A mismatch indicates a protocol bug and MUST be treated as a fatal error
(close the connection).

---

## 4. Request ID

| Property | Value |
|---|---|
| Starting value | **1** (not 0) |
| Increment | +1 per call, per connection |
| Type | `uint64` |
| Overflow | Wrap to 1 (not 0) — implementations MAY ignore this in practice |

---

## 5. Status Codes

Defined in `proto/ibridger/envelope.proto` as `StatusCode`.

| Code | Meaning |
|---|---|
| `OK` | Success |
| `NOT_FOUND` | Service or method does not exist |
| `INVALID_ARGUMENT` | Malformed request payload |
| `INTERNAL` | Unhandled exception in the handler |
| `TIMEOUT` | (future) call exceeded deadline |
| `UNKNOWN_ERROR` | Catch-all |

---

## 6. Built-in Ping Service

Every server MUST auto-register this service unless explicitly disabled.

| Property | Value |
|---|---|
| Service name | `"ibridger.Ping"` |
| Method name | `"Ping"` |
| Request type | `ibridger.Ping` proto |
| Response type | `ibridger.Pong` proto |
| `server_id` field | `"ibridger-server"` |
| `timestamp_ms` field | Current Unix epoch time in **milliseconds** |

The `server_id` value `"ibridger-server"` is verified by the cross-language
integration tests. Any SDK that returns a different value will fail.

---

## 7. Default Timeout

| Property | Value |
|---|---|
| Default call timeout | `DEFAULT_TIMEOUT_MS = 30000` ms (30 s) — from `constants.proto` |
| Behaviour on expiry | Close connection; surface as timeout error to caller |

---

## 8. Error Handling Contract

| Situation | Server MUST respond with |
|---|---|
| Unknown service | `ERROR` + `NOT_FOUND` + descriptive `error_message` |
| Unknown method | `ERROR` + `NOT_FOUND` + descriptive `error_message` |
| Handler throws / panics | `ERROR` + `INTERNAL` + exception message |
| Malformed request proto | `ERROR` + `INVALID_ARGUMENT` |

The server MUST NOT close the connection on a dispatch error — it MUST send
the error envelope and continue serving subsequent requests on that connection.

---

## 9. Conformance Tests

The cross-language integration tests in `tests/integration/` (Phase 22) are
the machine-readable enforcement of this document. Every new SDK MUST pass
them before being considered complete.
