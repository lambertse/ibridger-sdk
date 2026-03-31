import Long from 'long';
import { ibridger } from '../generated/proto';
import { UnixSocketConnection } from '../transport/unix-socket';
import { UnixSocketServer } from '../transport/unix-socket-server';
import { FramedConnection } from '../protocol/framing';
import { EnvelopeCodec } from '../protocol/envelope-codec';
import { ProtoType } from './client';

// ─── Handler types ────────────────────────────────────────────────────────────

/** Raw method handler — receives and returns serialized proto bytes. */
export type MethodHandler = (payload: Buffer) => Promise<Buffer>;

/** Service definition: a map of method name → raw handler. */
export interface ServiceDefinition {
  [methodName: string]: MethodHandler;
}

/**
 * Wraps a typed handler into a raw `MethodHandler`.
 *
 * Handles proto serialization/deserialization automatically so callers
 * work with domain objects rather than raw bytes.
 *
 * Example:
 *   server.register('EchoService', {
 *     Echo: typedMethod(EchoRequest, EchoResponse, async (req) => ({
 *       message: req.message.toUpperCase(),
 *     })),
 *   });
 */
export function typedMethod<TReq, TResp>(
  reqType: ProtoType<TReq>,
  respType: ProtoType<TResp>,
  handler: (req: TReq) => Promise<TResp>,
): MethodHandler {
  return async (payload: Buffer): Promise<Buffer> => {
    const req = reqType.decode(payload);
    const resp = await handler(req);
    return Buffer.from(respType.encode(resp).finish());
  };
}

// ─── Built-in string constants (canonical values from WIRE_PROTOCOL.md) ───────

const PING_SERVICE_NAME = 'ibridger.Ping';
const PING_METHOD_NAME  = 'Ping';
const PING_SERVER_ID    = 'ibridger-server';

// ─── IBridgerServer ───────────────────────────────────────────────────────────

export interface ServerConfig {
  endpoint: string;
  /** Auto-register the built-in Ping service. Defaults to true. */
  registerBuiltins?: boolean;
}

/**
 * ibridger RPC server for Node.js.
 *
 * Accepts Unix domain socket connections and dispatches incoming Envelope
 * requests to registered service handlers. Each connection runs its own
 * independent async dispatch loop — no threads, no shared mutable state
 * between connections.
 *
 * Example:
 *   const server = new IBridgerServer({ endpoint: '/tmp/my.sock' });
 *   server.register('EchoService', {
 *     Echo: typedMethod(EchoRequest, EchoResponse, async (req) => ({
 *       message: req.message.toUpperCase(),
 *     })),
 *   });
 *   await server.start();
 */
export class IBridgerServer {
  private readonly services = new Map<string, Map<string, MethodHandler>>();
  private socketServer: UnixSocketServer | null = null;
  private readonly activeConnections = new Set<UnixSocketConnection>();
  private _isRunning = false;

  constructor(private readonly config: ServerConfig) {
    if (config.registerBuiltins !== false) {
      this.registerBuiltinPing();
    }
  }

  /**
   * Register a service with one or more method handlers.
   * Calling register() multiple times with the same service name merges the methods.
   */
  register(serviceName: string, methods: ServiceDefinition): this {
    const existing = this.services.get(serviceName) ?? new Map<string, MethodHandler>();
    for (const [method, handler] of Object.entries(methods)) {
      existing.set(method, handler);
    }
    this.services.set(serviceName, existing);
    return this;
  }

  /** Start listening. Resolves once the server is bound and accepting. */
  async start(): Promise<void> {
    if (this._isRunning) throw new Error('Server is already running');
    this.socketServer = new UnixSocketServer(this.config.endpoint);
    await this.socketServer.listen((conn) => this.handleConnection(conn));
    this._isRunning = true;
  }

  /** Stop accepting new connections and close all active ones. */
  async stop(): Promise<void> {
    if (!this._isRunning) return;
    this._isRunning = false;

    // Close all active connections to unblock pending recv() calls.
    for (const conn of this.activeConnections) {
      conn.close();
    }
    this.activeConnections.clear();

    if (this.socketServer) {
      await this.socketServer.close();
      this.socketServer = null;
    }
  }

  get isRunning(): boolean {
    return this._isRunning;
  }

  // ─── Connection handling ────────────────────────────────────────────────────

  private handleConnection(conn: UnixSocketConnection): void {
    this.activeConnections.add(conn);
    // Fire-and-forget: the async loop runs until the connection closes.
    this.dispatchLoop(conn).finally(() => {
      this.activeConnections.delete(conn);
      conn.close();
    });
  }

  private async dispatchLoop(conn: UnixSocketConnection): Promise<void> {
    const framed = new FramedConnection(conn);
    const codec  = new EnvelopeCodec(framed);

    while (conn.isConnected) {
      let request: ibridger.Envelope;
      try {
        request = await codec.recv();
      } catch {
        break; // connection closed or framing error
      }

      const response = await this.dispatch(request);

      try {
        await codec.send(response);
      } catch {
        break; // connection closed while sending
      }
    }
  }

  // ─── Dispatch ──────────────────────────────────────────────────────────────

  private async dispatch(request: ibridger.Envelope): Promise<ibridger.Envelope> {
    const service = this.services.get(request.serviceName);
    if (!service) {
      return this.errorEnvelope(
        request,
        ibridger.StatusCode.NOT_FOUND,
        `Service not found: ${request.serviceName}`,
      );
    }

    const handler = service.get(request.methodName);
    if (!handler) {
      return this.errorEnvelope(
        request,
        ibridger.StatusCode.NOT_FOUND,
        `Method not found: ${request.serviceName}/${request.methodName}`,
      );
    }

    try {
      const responsePayload = await handler(Buffer.from(request.payload));
      return ibridger.Envelope.create({
        type:      ibridger.MessageType.RESPONSE,
        status:    ibridger.StatusCode.OK,
        requestId: request.requestId,
        payload:   responsePayload,
      });
    } catch (e) {
      return this.errorEnvelope(
        request,
        ibridger.StatusCode.INTERNAL,
        (e instanceof Error ? e.message : String(e)) || 'Internal server error',
      );
    }
  }

  private errorEnvelope(
    request: ibridger.Envelope,
    status: ibridger.StatusCode,
    message: string,
  ): ibridger.Envelope {
    return ibridger.Envelope.create({
      type:         ibridger.MessageType.ERROR,
      status,
      requestId:    request.requestId,
      errorMessage: message,
    });
  }

  // ─── Built-in Ping ─────────────────────────────────────────────────────────

  private registerBuiltinPing(): void {
    this.register(PING_SERVICE_NAME, {
      [PING_METHOD_NAME]: typedMethod(
        ibridger.Ping as unknown as ProtoType<ibridger.Ping>,
        ibridger.Pong as unknown as ProtoType<ibridger.Pong>,
        async (_req: ibridger.Ping) =>
          ibridger.Pong.create({
            serverId:    PING_SERVER_ID,
            timestampMs: Long.fromNumber(Date.now()),
          }),
      ),
    });
  }
}
