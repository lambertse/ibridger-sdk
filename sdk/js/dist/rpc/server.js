"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.IBridgerServer = void 0;
exports.typedMethod = typedMethod;
const long_1 = __importDefault(require("long"));
const proto_1 = require("../generated/proto");
const unix_socket_server_1 = require("../transport/unix-socket-server");
const framing_1 = require("../protocol/framing");
const envelope_codec_1 = require("../protocol/envelope-codec");
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
function typedMethod(reqType, respType, handler) {
    return async (payload) => {
        const req = reqType.decode(payload);
        const resp = await handler(req);
        return Buffer.from(respType.encode(resp).finish());
    };
}
// ─── Built-in string constants (canonical values from WIRE_PROTOCOL.md) ───────
const PING_SERVICE_NAME = 'ibridger.Ping';
const PING_METHOD_NAME = 'Ping';
const PING_SERVER_ID = 'ibridger-server';
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
class IBridgerServer {
    config;
    services = new Map();
    socketServer = null;
    activeConnections = new Set();
    _isRunning = false;
    constructor(config) {
        this.config = config;
        if (config.registerBuiltins !== false) {
            this.registerBuiltinPing();
        }
    }
    /**
     * Register a service with one or more method handlers.
     * Calling register() multiple times with the same service name merges the methods.
     */
    register(serviceName, methods) {
        const existing = this.services.get(serviceName) ?? new Map();
        for (const [method, handler] of Object.entries(methods)) {
            existing.set(method, handler);
        }
        this.services.set(serviceName, existing);
        return this;
    }
    /** Start listening. Resolves once the server is bound and accepting. */
    async start() {
        if (this._isRunning)
            throw new Error('Server is already running');
        this.socketServer = new unix_socket_server_1.UnixSocketServer(this.config.endpoint);
        await this.socketServer.listen((conn) => this.handleConnection(conn));
        this._isRunning = true;
    }
    /** Stop accepting new connections and close all active ones. */
    async stop() {
        if (!this._isRunning)
            return;
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
    get isRunning() {
        return this._isRunning;
    }
    // ─── Connection handling ────────────────────────────────────────────────────
    handleConnection(conn) {
        this.activeConnections.add(conn);
        // Fire-and-forget: the async loop runs until the connection closes.
        this.dispatchLoop(conn).finally(() => {
            this.activeConnections.delete(conn);
            conn.close();
        });
    }
    async dispatchLoop(conn) {
        const framed = new framing_1.FramedConnection(conn);
        const codec = new envelope_codec_1.EnvelopeCodec(framed);
        while (conn.isConnected) {
            let request;
            try {
                request = await codec.recv();
            }
            catch {
                break; // connection closed or framing error
            }
            const response = await this.dispatch(request);
            try {
                await codec.send(response);
            }
            catch {
                break; // connection closed while sending
            }
        }
    }
    // ─── Dispatch ──────────────────────────────────────────────────────────────
    async dispatch(request) {
        const service = this.services.get(request.serviceName);
        if (!service) {
            return this.errorEnvelope(request, proto_1.ibridger.StatusCode.NOT_FOUND, `Service not found: ${request.serviceName}`);
        }
        const handler = service.get(request.methodName);
        if (!handler) {
            return this.errorEnvelope(request, proto_1.ibridger.StatusCode.NOT_FOUND, `Method not found: ${request.serviceName}/${request.methodName}`);
        }
        try {
            const responsePayload = await handler(Buffer.from(request.payload));
            return proto_1.ibridger.Envelope.create({
                type: proto_1.ibridger.MessageType.RESPONSE,
                status: proto_1.ibridger.StatusCode.OK,
                requestId: request.requestId,
                payload: responsePayload,
            });
        }
        catch (e) {
            return this.errorEnvelope(request, proto_1.ibridger.StatusCode.INTERNAL, (e instanceof Error ? e.message : String(e)) || 'Internal server error');
        }
    }
    errorEnvelope(request, status, message) {
        return proto_1.ibridger.Envelope.create({
            type: proto_1.ibridger.MessageType.ERROR,
            status,
            requestId: request.requestId,
            errorMessage: message,
        });
    }
    // ─── Built-in Ping ─────────────────────────────────────────────────────────
    registerBuiltinPing() {
        this.register(PING_SERVICE_NAME, {
            [PING_METHOD_NAME]: typedMethod(proto_1.ibridger.Ping, proto_1.ibridger.Pong, async (_req) => proto_1.ibridger.Pong.create({
                serverId: PING_SERVER_ID,
                timestampMs: long_1.default.fromNumber(Date.now()),
            })),
        });
    }
}
exports.IBridgerServer = IBridgerServer;
//# sourceMappingURL=server.js.map
