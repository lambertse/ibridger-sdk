import { ibridger } from '../generated/proto';
export interface CallOptions {
    /** Timeout in milliseconds. Defaults to 30 000. */
    timeout?: number;
    metadata?: Record<string, string>;
}
/** Codec interface extracted for testability. */
export interface ICodec {
    send(envelope: ibridger.Envelope): Promise<void>;
    recv(): Promise<ibridger.Envelope>;
}
/** Thrown when the server responds with a non-OK status. */
export declare class RpcError extends Error {
    readonly status: ibridger.StatusCode;
    constructor(status: ibridger.StatusCode, message: string);
}
/** Thrown when a call exceeds its timeout. */
export declare class TimeoutError extends Error {
    constructor(timeoutMs: number);
}
/** Structural type for a protobuf static class (encode + decode). */
export interface ProtoType<T> {
    encode(message: T): {
        finish(): Uint8Array;
    };
    decode(data: Uint8Array): T;
}
/**
 * High-level RPC client for the ibridger-sdk wire protocol.
 *
 * - Maintains a single connection (connect/disconnect lifecycle).
 * - Serializes calls — one outstanding request at a time.
 * - Generates monotonically increasing request IDs and validates correlation.
 * - Supports per-call timeouts and metadata.
 */
export declare class IBridgerClient {
    private readonly config;
    private codec;
    private nextRequestId;
    private callInFlight;
    constructor(config: {
        endpoint: string;
        defaultTimeout?: number;
    });
    connect(): Promise<void>;
    disconnect(): void;
    get isConnected(): boolean;
    /**
     * Make a typed RPC call.
     *
     * Serializes `request` using `reqType.encode`, sends it, awaits the response,
     * then deserializes with `respType.decode`.
     *
     * Throws `RpcError` if the server returns a non-OK status.
     * Throws `TimeoutError` if `options.timeout` ms elapse before a reply.
     */
    call<TReq, TResp>(service: string, method: string, request: TReq, reqType: ProtoType<TReq>, respType: ProtoType<TResp>, options?: CallOptions): Promise<TResp>;
    /** Convenience: ping the built-in ibridger.Ping service. */
    ping(options?: CallOptions): Promise<ibridger.Pong>;
    private withTimeout;
    /** Returns codec cast to a closeable shape, if it supports close(). */
    private codecAsCloseable;
}
//# sourceMappingURL=client.d.ts.map
