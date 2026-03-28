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
// To run the server, execute "npm start" in this directory ("npm install" first).
// By default it listens on localhost:8000, can be changed via PORT environment variable
// to either a port number (e.g. "9000") or a host:port pair (e.g. "0.0.0.0:9000").

import { createServer } from 'http';
import { server } from 'websocket';

// Registry of currently connected peers: { peerId -> WebSocketConnection }
const clients = {};

// ---------------------------------------------------------------------------
// HTTP server — exists solely to give the WebSocket server an underlying
// TCP listener. All plain HTTP requests are rejected with 404.
// ---------------------------------------------------------------------------
const httpServer = createServer((req, res) => {
  console.log(`HTTP ${req.method.toUpperCase()} ${req.url}`);
  res.writeHead(404, {
    'Content-Type'                : 'text/plain',
    'Access-Control-Allow-Origin' : '*',
  });
  res.end('Not Found');
});

// ---------------------------------------------------------------------------
// WebSocket server — handles peer registration and message relay.
// ---------------------------------------------------------------------------
const wsServer = new server({httpServer});

wsServer.on('request', (req) => {
  // Extract the peer ID from the URL path: ws://host/peerId
  const pathSegments = req.resourceURL.path.split('/');
  pathSegments.shift(); // remove leading empty string from '/'
  const senderId = pathSegments[0];

  console.log(`WS  connect  [${senderId}]  ${req.resource}`);

  // Accept the connection regardless of subprotocol.
  const conn = req.accept(null, req.origin);

  // Register this peer so others can send messages to it.
  clients[senderId] = conn;

  // -------------------------------------------------------------------------
  // Relay incoming message to the target peer.
  // Expected format: { "id": "<targetPeerId>", ...signalingPayload }
  // -------------------------------------------------------------------------
  conn.on('message', (raw) => {
    if (raw.type !== 'utf8') return; // binary frames are not used

    console.log(`WS  recv  [${senderId}] << ${raw.utf8Data}`);

    const message  = JSON.parse(raw.utf8Data);
    const targetId = message.id;
    const target   = clients[targetId];

    if (!target) {
      console.error(`WS  relay FAILED: peer [${targetId}] not connected`);
      return;
    }

    // Replace destination ID with sender ID so the receiver knows the origin.
    message.id = senderId;
    const outgoing = JSON.stringify(message);

    console.log(`WS  relay [${senderId}] -> [${targetId}] >> ${outgoing}`);
    target.send(outgoing);
  });

  // -------------------------------------------------------------------------
  // Unregister the peer when the connection closes.
  // -------------------------------------------------------------------------
  conn.on('close', () => {
    delete clients[senderId];
    console.log(`WS  disconnect [${senderId}]`);
  });
});

// ---------------------------------------------------------------------------
// Startup — resolve listen address from the PORT environment variable.
// Formats accepted: "8000" (port only) or "0.0.0.0:8000" (host:port).
// ---------------------------------------------------------------------------
const endpoint = process.env.PORT || '8000';
const parts    = endpoint.split(':');
const port     = parts.pop();
const hostname = parts.join(':') || '127.0.0.1';

httpServer.listen(port, hostname, () => {
  console.log(`Signaling server listening on ${hostname}:${port}`);
});
