/**
 * Cross-language integration tests.
 *
 * Each scenario runs against two server implementations:
 *   - C++ echo_server  (sdk/cpp/build/echo_server)
 *   - JS  echo-server  (sdk/js/examples/echo-server.ts via ts-node)
 *
 * This proves that the wire protocol is compatible across languages and that
 * both sides conform to docs/WIRE_PROTOCOL.md.
 */

import Long from 'long';
import {
  IBridgerClient,
  RpcError,
  ProtoType,
} from '../../sdk/js/src/rpc/client';
import { ibridger } from '../../sdk/js/src/generated/proto';
import {
  tempSocketPath,
  startCppServer,
  startJsServer,
  ServerHandle,
} from './helpers';

// ─── Shared proto type aliases ────────────────────────────────────────────────

type EchoReq = ibridger.examples.EchoRequest;
type EchoRes = ibridger.examples.EchoResponse;
const EchoReq = ibridger.examples.EchoRequest  as unknown as ProtoType<EchoReq>;
const EchoRes = ibridger.examples.EchoResponse as unknown as ProtoType<EchoRes>;

// ─── Helpers ──────────────────────────────────────────────────────────────────

async function connectClient(endpoint: string): Promise<IBridgerClient> {
  const client = new IBridgerClient({ endpoint });
  await client.connect();
  return client;
}

// ─── Test factory — runs the same suite against any server type ───────────────

function defineScenarios(
  label: string,
  startServer: (socketPath: string) => Promise<ServerHandle>,
) {
  describe(label, () => {
    let server: ServerHandle;
    let socketPath: string;

    beforeEach(async () => {
      socketPath = tempSocketPath();
      server = await startServer(socketPath);
    });

    afterEach(async () => {
      await server.stop();
    });

    // ── 1. JS client pings server ───────────────────────────────────────────

    it('JS client pings server — returns correct Pong', async () => {
      const client = await connectClient(socketPath);
      const before = Date.now();

      const pong = await client.ping();

      expect(pong.serverId).toBe('ibridger-server');
      expect(Number(pong.timestampMs)).toBeGreaterThanOrEqual(before);
      // Timestamp within 1 second of "now".
      expect(Number(pong.timestampMs)).toBeLessThanOrEqual(Date.now() + 1000);

      client.disconnect();
    });

    // ── 2. JS client calls EchoService — verify uppercase response ──────────

    it('JS client echo — response is uppercased', async () => {
      const client = await connectClient(socketPath);

      const response = await client.call<EchoReq, EchoRes>(
        'EchoService', 'Echo',
        ibridger.examples.EchoRequest.create({ message: 'hello integration' }),
        EchoReq, EchoRes,
      );

      expect(response.message).toBe('HELLO INTEGRATION');
      expect(Number(response.timestampMs)).toBeGreaterThan(0);

      client.disconnect();
    });

    // ── 3. Error propagation: NOT_FOUND for unknown service ─────────────────

    it('unknown service returns NOT_FOUND RpcError', async () => {
      const client = await connectClient(socketPath);

      try {
        await client.call<EchoReq, EchoRes>(
          'ghost.Service', 'Nope',
          ibridger.examples.EchoRequest.create({ message: '' }),
          EchoReq, EchoRes,
        );
        fail('expected RpcError');
      } catch (e) {
        expect(e).toBeInstanceOf(RpcError);
        expect((e as RpcError).status).toBe(ibridger.StatusCode.NOT_FOUND);
      }

      client.disconnect();
    });

    // ── 4. Multiple concurrent JS clients ───────────────────────────────────

    it('multiple concurrent JS clients all succeed', async () => {
      const N = 5;
      const clients = await Promise.all(
        Array.from({ length: N }, () => connectClient(socketPath)),
      );

      const results = await Promise.all(
        clients.map((c, i) =>
          c.call<EchoReq, EchoRes>(
            'EchoService', 'Echo',
            ibridger.examples.EchoRequest.create({ message: `client-${i}` }),
            EchoReq, EchoRes,
          ),
        ),
      );

      results.forEach((r, i) => {
        expect(r.message).toBe(`CLIENT-${i}`);
      });

      await Promise.all(clients.map((c) => c.disconnect()));
    });

    // ── 5. Client reconnect after disconnect ────────────────────────────────

    it('client reconnects and calls successfully after disconnect', async () => {
      const client = await connectClient(socketPath);

      const r1 = await client.call<EchoReq, EchoRes>(
        'EchoService', 'Echo',
        ibridger.examples.EchoRequest.create({ message: 'first' }),
        EchoReq, EchoRes,
      );
      expect(r1.message).toBe('FIRST');

      client.disconnect();

      await client.connect();

      const r2 = await client.call<EchoReq, EchoRes>(
        'EchoService', 'Echo',
        ibridger.examples.EchoRequest.create({ message: 'second' }),
        EchoReq, EchoRes,
      );
      expect(r2.message).toBe('SECOND');

      client.disconnect();
    });

    // ── 6. Multiple sequential calls on one connection ──────────────────────

    it('multiple sequential calls on one connection all succeed', async () => {
      const client = await connectClient(socketPath);

      for (let i = 0; i < 10; i++) {
        const r = await client.call<EchoReq, EchoRes>(
          'EchoService', 'Echo',
          ibridger.examples.EchoRequest.create({ message: `msg-${i}` }),
          EchoReq, EchoRes,
        );
        expect(r.message).toBe(`MSG-${i}`);
      }

      client.disconnect();
    });
  });
}

// ─── Run against both server implementations ──────────────────────────────────

defineScenarios('C++ server ↔ JS client', startCppServer);
defineScenarios('JS server ↔ JS client', startJsServer);
