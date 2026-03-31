"use strict";
// ibridger-sdk JS/TypeScript SDK — public API
Object.defineProperty(exports, "__esModule", { value: true });
exports.ibridger = exports.EnvelopeCodec = exports.FramedConnection = exports.TransportError = exports.UnixSocketConnection = exports.UnixSocketServer = exports.typedMethod = exports.IBridgerServer = exports.TimeoutError = exports.RpcError = exports.IBridgerClient = void 0;
var client_1 = require("./rpc/client");
Object.defineProperty(exports, "IBridgerClient", { enumerable: true, get: function () { return client_1.IBridgerClient; } });
Object.defineProperty(exports, "RpcError", { enumerable: true, get: function () { return client_1.RpcError; } });
Object.defineProperty(exports, "TimeoutError", { enumerable: true, get: function () { return client_1.TimeoutError; } });
var server_1 = require("./rpc/server");
Object.defineProperty(exports, "IBridgerServer", { enumerable: true, get: function () { return server_1.IBridgerServer; } });
Object.defineProperty(exports, "typedMethod", { enumerable: true, get: function () { return server_1.typedMethod; } });
var unix_socket_server_1 = require("./transport/unix-socket-server");
Object.defineProperty(exports, "UnixSocketServer", { enumerable: true, get: function () { return unix_socket_server_1.UnixSocketServer; } });
var unix_socket_1 = require("./transport/unix-socket");
Object.defineProperty(exports, "UnixSocketConnection", { enumerable: true, get: function () { return unix_socket_1.UnixSocketConnection; } });
var types_1 = require("./transport/types");
Object.defineProperty(exports, "TransportError", { enumerable: true, get: function () { return types_1.TransportError; } });
var framing_1 = require("./protocol/framing");
Object.defineProperty(exports, "FramedConnection", { enumerable: true, get: function () { return framing_1.FramedConnection; } });
var envelope_codec_1 = require("./protocol/envelope-codec");
Object.defineProperty(exports, "EnvelopeCodec", { enumerable: true, get: function () { return envelope_codec_1.EnvelopeCodec; } });
var proto_1 = require("./generated/proto");
Object.defineProperty(exports, "ibridger", { enumerable: true, get: function () { return proto_1.ibridger; } });
//# sourceMappingURL=index.js.map
