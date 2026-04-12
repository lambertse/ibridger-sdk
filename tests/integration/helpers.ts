import * as child_process from 'child_process';
import * as fs from 'fs';
import * as os from 'os';
import * as path from 'path';

const REPO_ROOT = path.resolve(__dirname, '../..');
const CPP_ECHO_SERVER = path.join(REPO_ROOT, 'sdk/cpp/build/echo_server');
const JS_ECHO_SERVER  = path.join(REPO_ROOT, 'sdk/js/examples/echo-server.ts');
const TS_NODE         = path.join(REPO_ROOT, 'sdk/js/node_modules/.bin/ts-node');

export function tempSocketPath(): string {
  return path.join(os.tmpdir(), `ibridger_integ_${process.pid}_${Date.now()}.sock`);
}

// ─── Process-based server (C++ binary or ts-node script) ─────────────────────

export interface ServerHandle {
  stop(): Promise<void>;
}

function spawnServer(
  cmd: string,
  args: string[],
  socketPath: string,
  label: string,
): Promise<ServerHandle> {
  return new Promise((resolve, reject) => {
    const proc = child_process.spawn(cmd, args, {
      stdio: ['ignore', 'pipe', 'pipe'],
    });

    let resolved = false;

    // Poll for socket file — resolves when the server is accepting connections.
    const poll = setInterval(() => {
      if (fs.existsSync(socketPath)) {
        clearInterval(poll);
        if (!resolved) {
          resolved = true;
          resolve({
            stop: () =>
              new Promise<void>((res) => {
                proc.kill('SIGTERM');
                proc.once('exit', () => {
                  try { fs.unlinkSync(socketPath); } catch {}
                  res();
                });
              }),
          });
        }
      }
    }, 30);

    proc.once('error', (err) => {
      clearInterval(poll);
      if (!resolved) reject(new Error(`${label} spawn error: ${err.message}`));
    });

    proc.once('exit', (code) => {
      clearInterval(poll);
      if (!resolved) reject(new Error(`${label} exited early with code ${code}`));
    });

    // Safety timeout — fail if socket doesn't appear within 10 s.
    setTimeout(() => {
      clearInterval(poll);
      if (!resolved) {
        resolved = true;
        proc.kill();
        reject(new Error(`${label} did not start within 10 s`));
      }
    }, 10_000);
  });
}

/** Start the C++ echo_server binary. */
export function startCppServer(socketPath: string): Promise<ServerHandle> {
  return spawnServer(CPP_ECHO_SERVER, [socketPath], socketPath, 'C++ echo_server');
}

/** Start the JS echo-server.ts via ts-node. */
export function startJsServer(socketPath: string): Promise<ServerHandle> {
  return spawnServer(TS_NODE, [JS_ECHO_SERVER, socketPath], socketPath, 'JS echo-server');
}
