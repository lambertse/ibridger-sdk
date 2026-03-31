import { ProtoType } from './client';
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
export declare function typedMethod<TReq, TResp>(reqType: ProtoType<TReq>, respType: ProtoType<TResp>, handler: (req: TReq) => Promise<TResp>): MethodHandler;
export interface ServerConfig {
    endpoint: string;
    /** Auto-register the built-in Ping service. Defaults to true. */
    registerBuiltins?: boolean;
}
/**
 * ibridger-sdk RPC server for Node.js.
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
export declare class IBridgerServer {
    private readonly config;
    private readonly services;
    private socketServer;
    private readonly activeConnections;
    private _isRunning;
    constructor(config: ServerConfig);
    /**
     * Register a service with one or more method handlers.
     * Calling register() multiple times with the same service name merges the methods.
     */
    register(serviceName: string, methods: ServiceDefinition): this;
    /** Start listening. Resolves once the server is bound and accepting. */
    start(): Promise<void>;
    /** Stop accepting new connections and close all active ones. */
    stop(): Promise<void>;
    get isRunning(): boolean;
    private handleConnection;
    private dispatchLoop;
    private dispatch;
    private errorEnvelope;
    private registerBuiltinPing;
}
//# sourceMappingURL=server.d.ts.map
