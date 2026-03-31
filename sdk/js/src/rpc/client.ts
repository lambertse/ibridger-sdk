import Long from 'long';
import { ibridger } from '../generated/proto';
import { UnixSocketConnection } from '../transport/unix-socket';
import { FramedConnection } from '../protocol/framing';
import { EnvelopeCodec } from '../protocol/envelope-codec';

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
export class RpcError extends Error {
  constructor(
    public readonly status: ibridger.StatusCode,
    message: string,
  ) {
    super(message);
    this.name = 'RpcError';
  }
}

/** Thrown when a call exceeds its timeout. */
export class TimeoutError extends Error {
  constructor(timeoutMs: number) {
    super(`RPC call timed out after ${timeoutMs} ms`);
    this.name = 'TimeoutError';
  }
}

/** Structural type for a protobuf static class (encode + decode). */
export interface ProtoType<T> {
  encode(message: T): { finish(): Uint8Array };
  decode(data: Uint8Array): T;
}

const DEFAULT_TIMEOUT_MS = ibridger.WireConstant.DEFAULT_TIMEOUT_MS;

/**
 * High-level RPC client for the ibridger wire protocol.
 *
 * - Maintains a single connection (connect/disconnect lifecycle).
 * - Serializes calls — one outstanding request at a time.
 * - Generates monotonically increasing request IDs and validates correlation.
 * - Supports per-call timeouts and metadata.
 */
export class IBridgerClient {
  private codec: ICodec | null = null;
  private nextRequestId = 1;
  private callInFlight = false;

  constructor(private readonly config: { endpoint: string; defaultTimeout?: number }) {}

  async connect(): Promise<void> {
    if (this.codec) throw new Error('Already connected');
    const conn = await UnixSocketConnection.connect(this.config.endpoint);
    this.codec = new EnvelopeCodec(new FramedConnection(conn));
  }

  disconnect(): void {
    // EnvelopeCodec → FramedConnection → UnixSocketConnection.close()
    // We reach through the layers via a close helper below.
    if (this.codecAsCloseable()) this.codecAsCloseable()!.close();
    this.codec = null;
  }

  get isConnected(): boolean {
    return this.codec !== null;
  }

  /**
   * Make a typed RPC call.
   *
   * Serializes `request` using `reqType.encode`, sends it, awaits the response,
   * then deserializes with `respType.decode`.
   *
   * Throws `RpcError` if the server returns a non-OK status.
   * Throws `TimeoutError` if `options.timeout` ms elapse before a reply.
   */
  async call<TReq, TResp>(
    service: string,
    method: string,
    request: TReq,
    reqType: ProtoType<TReq>,
    respType: ProtoType<TResp>,
    options?: CallOptions,
  ): Promise<TResp> {
    if (!this.codec) throw new Error('Not connected');
    if (this.callInFlight) throw new Error('A call is already in flight');

    const payload = Buffer.from(reqType.encode(request).finish());
    const id = this.nextRequestId++;
    const timeoutMs = options?.timeout ?? this.config.defaultTimeout ?? DEFAULT_TIMEOUT_MS;

    const envelope = ibridger.Envelope.create({
      type:        ibridger.MessageType.REQUEST,
      requestId:   Long.fromNumber(id),
      serviceName: service,
      methodName:  method,
      payload,
      metadata:    options?.metadata ?? {},
    });

    this.callInFlight = true;
    try {
      await this.codec.send(envelope);
      const response = await this.withTimeout(this.codec.recv(), timeoutMs);

      if (Number(response.requestId) !== id) {
        throw new Error(`Protocol error: expected request_id ${id}, got ${Number(response.requestId)}`);
      }

      if (response.status !== ibridger.StatusCode.OK) {
        throw new RpcError(response.status, response.errorMessage || `RPC failed with status ${response.status}`);
      }

      return respType.decode(Buffer.from(response.payload));
    } finally {
      this.callInFlight = false;
    }
  }

  /** Convenience: ping the built-in ibridger.Ping service. */
  async ping(options?: CallOptions): Promise<ibridger.Pong> {
    return this.call(
      'ibridger.Ping',
      'Ping',
      ibridger.Ping.create({}),
      ibridger.Ping as unknown as ProtoType<ibridger.Ping>,
      ibridger.Pong as unknown as ProtoType<ibridger.Pong>,
      options,
    );
  }

  // ─── internal ──────────────────────────────────────────────────────────────

  private withTimeout<T>(promise: Promise<T>, ms: number): Promise<T> {
    return new Promise<T>((resolve, reject) => {
      const timer = setTimeout(() => reject(new TimeoutError(ms)), ms);
      promise.then(
        (v) => { clearTimeout(timer); resolve(v); },
        (e) => { clearTimeout(timer); reject(e); },
      );
    });
  }

  /** Returns codec cast to a closeable shape, if it supports close(). */
  private codecAsCloseable(): { close(): void } | null {
    const c = this.codec as unknown as { close?: () => void };
    return typeof c?.close === 'function' ? (c as { close(): void }) : null;
  }
}
