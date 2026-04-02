// WebRTC Signaling Server
//
// Purpose: relay arbitrary JSON messages (SDP offers/answers, ICE candidates,
// etc.) between WebRTC peers. This server never inspects the WebRTC payload —
// it just routes messages from one client to another by peer ID.
//
// Connection protocol:
//   ws://<host>:<port>/<myId>
//
// Message protocol (JSON, both directions):
//   { "id": "<targetId>", ...payload }
//
// On receipt the server replaces "id" with the sender's ID, so the receiver
// always knows who sent the message:
//   { "id": "<senderId>", ...payload }
//
// To run the server, execute "npm start" or "bazel run //demo/pkg/rtt/sig-service".
// By default it listens on localhost:8000, can be changed via PORT environment variable
// to either a port number (e.g. "9000") or a host:port pair (e.g. "0.0.0.0:9000").

import { createServer } from 'http';
import { server } from 'websocket';

const clients = {};

const PENDING_TTL_MS = 10_000;
const pending = {};

function enqueuePending(targetId, senderId, message) {
  if (!pending[targetId]) {
    pending[targetId] = [];
    setTimeout(() => {
      if (pending[targetId]) {
        console.warn(`WS  pending TTL expired for [${targetId}], dropping ${pending[targetId].length} message(s)`);
        delete pending[targetId];
      }
    }, PENDING_TTL_MS);
  }
  console.log(`WS  pending [${senderId}] -> [${targetId}] (queued, not connected yet)`);
  pending[targetId].push({ senderId, message });
}

function flushPending(targetId, conn) {
  const msgs = pending[targetId];
  if (!msgs || msgs.length === 0) return;
  delete pending[targetId];
  console.log(`WS  pending flush [${targetId}]: delivering ${msgs.length} queued message(s)`);
  for (const { senderId, message } of msgs) {
    message.id = senderId;
    const outgoing = JSON.stringify(message);
    console.log(`WS  relay (flushed) [${senderId}] -> [${targetId}] >> ${outgoing}`);
    conn.send(outgoing);
  }
}

const httpServer = createServer((req, res) => {
  if (req.url === '/health') {
    res.writeHead(200, {'Content-Type': 'text/plain', 'Access-Control-Allow-Origin': '*'});
    res.end('OK');
    return;
  }
  console.log(`HTTP ${req.method.toUpperCase()} ${req.url}`);
  res.writeHead(404, {
    'Content-Type'                : 'text/plain',
    'Access-Control-Allow-Origin' : '*',
  });
  res.end('Not Found');
});

const wsServer = new server({httpServer});

wsServer.on('request', (req) => {
  const pathSegments = req.resourceURL.path.split('/');
  pathSegments.shift();
  const senderId = pathSegments[0];

  console.log(`WS  connect  [${senderId}]  ${req.resource}`);

  const conn = req.accept(null, req.origin);

  clients[senderId] = conn;

  flushPending(senderId, conn);

  conn.on('message', (raw) => {
    if (raw.type !== 'utf8') return;

    console.log(`WS  recv  [${senderId}] << ${raw.utf8Data}`);

    const message  = JSON.parse(raw.utf8Data);
    const targetId = message.id;
    const target   = clients[targetId];

    if (!target) {
      console.error(`WS  relay FAILED: peer [${targetId}] not connected — queuing`);
      enqueuePending(targetId, senderId, message);
      return;
    }

    message.id = senderId;
    const outgoing = JSON.stringify(message);

    console.log(`WS  relay [${senderId}] -> [${targetId}] >> ${outgoing}`);
    target.send(outgoing);
  });

  conn.on('close', () => {
    delete clients[senderId];
    console.log(`WS  disconnect [${senderId}]`);
  });
});

const endpoint = process.env.PORT || '8000';
const parts    = endpoint.split(':');
const port     = parts.pop();
const hostname = parts.join(':') || '127.0.0.1';

httpServer.listen(port, hostname, () => {
  console.log(`Signaling server listening on ${hostname}:${port}`);
});
