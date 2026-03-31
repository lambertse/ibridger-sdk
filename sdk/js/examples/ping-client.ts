/**
 * ping-client.ts — Connect to a running ibridger server, send a Ping,
 * and print the server ID and round-trip latency.
 *
 * Usage:
 *   npx ts-node examples/ping-client.ts [socket-path]
 *
 * Default socket path: /tmp/ibridger_echo.sock
 *
 * Start the server first:
 *   npx ts-node examples/echo-server.ts
 */

import { IBridgerClient } from '../src/rpc/client';

const endpoint = process.argv[2] ?? '/tmp/ibridger_echo.sock';

async function main() {
  const client = new IBridgerClient({ endpoint });

  console.log(`Connecting to ${endpoint} ...`);
  await client.connect();
  console.log('Connected.');

  const t0 = Date.now();
  const pong = await client.ping();
  const latencyMs = Date.now() - t0;

  console.log(`Pong from server_id : ${pong.serverId}`);
  console.log(`Server timestamp    : ${Number(pong.timestampMs)} ms`);
  console.log(`Round-trip latency  : ${latencyMs} ms`);

  client.disconnect();
}

main().catch((err) => {
  console.error('Error:', err.message);
  process.exit(1);
});
