// ibridger JS/TypeScript SDK — public API

export { IBridgerClient, RpcError, TimeoutError } from './rpc/client';
export type { CallOptions, ICodec, ProtoType } from './rpc/client';

export { IBridgerServer, typedMethod } from './rpc/server';
export type { MethodHandler, ServiceDefinition, ServerConfig } from './rpc/server';

export { UnixSocketServer } from './transport/unix-socket-server';

export { UnixSocketConnection } from './transport/unix-socket';
export { TransportError } from './transport/types';
export type { IConnection } from './transport/types';

export { FramedConnection } from './protocol/framing';
export { EnvelopeCodec } from './protocol/envelope-codec';

export { ibridger } from './generated/proto';
