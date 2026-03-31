/**
 * echo-server.ts — Pure JS/TypeScript ibridger server exposing two services:
 *   - EchoService/Echo  : uppercases the message and stamps a timestamp
 *   - ibridger.Ping/Ping: built-in health check (auto-registered)
 *
 * Usage:
 *   npx ts-node examples/echo-server.ts [socket-path]
 *
 * Default socket path: /tmp/ibridger_echo.sock
 */

import { IBridgerServer, typedMethod } from '../src/rpc/server';
import { ibridger } from '../src/generated/proto';
import { ProtoType } from '../src/rpc/client';
import Long from 'long';

const endpoint = process.argv[2] ?? '/tmp/ibridger_echo.sock';

// Convenience alias for the generated echo types.
type EchoRequest  = ibridger.examples.EchoRequest;
type EchoResponse = ibridger.examples.EchoResponse;
const EchoRequest  = ibridger.examples.EchoRequest  as unknown as ProtoType<EchoRequest>;
const EchoResponse = ibridger.examples.EchoResponse as unknown as ProtoType<EchoResponse>;

const server = new IBridgerServer({ endpoint });

server.register('EchoService', {
  Echo: typedMethod<EchoRequest, EchoResponse>(
    EchoRequest,
    EchoResponse,
    async (req) => {
      console.log(`  Echo  <- "${req.message}"`);
      const upper = req.message.toUpperCase();
      console.log(`  Echo  -> "${upper}"`);
      return ibridger.examples.EchoResponse.create({
        message:     upper,
        timestampMs: Long.fromNumber(Date.now()),
      });
    },
  ),
});

async function main() {
  await server.start();
  console.log(`JS echo server listening on ${endpoint}`);
  console.log('Services: EchoService/Echo, ibridger.Ping/Ping');
  console.log('Press Ctrl-C to stop.\n');

  process.on('SIGINT',  shutdown);
  process.on('SIGTERM', shutdown);
}

async function shutdown() {
  console.log('\nStopping server...');
  await server.stop();
  console.log('Server stopped.');
  process.exit(0);
}

main().catch((err) => {
  console.error('Fatal:', err.message);
  process.exit(1);
});
