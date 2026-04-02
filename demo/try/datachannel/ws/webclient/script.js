// WebRTC DataChannel Demo — Browser Client
//
// Overall flow:
//   1. On load: generate a random local peer ID and connect to the signaling
//      server via WebSocket at ws://localhost:8000/<localId>.
//   2. OFFERER side: user types the remote peer's ID and clicks "Offer".
//      → Creates RTCPeerConnection + DataChannel → sends SDP offer via WS.
//   3. ANSWERER side: receives an offer via WS (no user action required).
//      → Creates RTCPeerConnection → sends SDP answer via WS.
//      → Receives the DataChannel through the ondatachannel event.
//   4. ICE candidates are forwarded through the signaling server as they are
//      discovered (trickle ICE). Each candidate is sent immediately instead of
//      waiting for full gathering, so connectivity checks run in parallel and
//      the connection is established faster.
//   5. Once the DataChannel is open both sides can exchange text messages
//      directly P2P — the signaling server is no longer involved.
//
// To run local web-server can be used:
//   python3 -m http.server --bind 127.0.0.1 8080

window.addEventListener('load', () => {

  // ---------------------------------------------------------------------------
  // Configuration & State
  // ---------------------------------------------------------------------------

  // RTCPeerConnection config: STUN server is used for NAT traversal (ICE).
  const config = {
    iceServers: [{ urls: 'stun:stun.l.google.com:19302' }],
  };

  // Each browser tab gets a random ID; the signaling server uses it as an
  // address to route messages.
  const localId = randomId(4);

  // Active connections keyed by remote peer ID.
  // Populated both on the offerer and answerer side.
  const peerConnectionMap = {};  // peerId -> RTCPeerConnection
  const dataChannelMap    = {};  // peerId -> RTCDataChannel

  // ---------------------------------------------------------------------------
  // UI References
  // ---------------------------------------------------------------------------

  const offerIdInput = document.getElementById('offerId');
  const offerBtn     = document.getElementById('offerBtn');
  const sendMsgInput = document.getElementById('sendMsg');
  const sendBtn      = document.getElementById('sendBtn');

  // Display our own ID so the remote peer can target us.
  document.getElementById('localId').textContent = localId;

  // ---------------------------------------------------------------------------
  // Startup: connect to signaling server, then unlock the offer UI.
  // ---------------------------------------------------------------------------

  console.log(`Local ID: ${localId} — connecting to signaling...`);

  openSignaling(`ws://localhost:8000/${localId}`)
    .then((ws) => {
      console.log('Signaling WebSocket ready');

      // Only enable the offer controls once the signaling channel is live.
      offerIdInput.disabled = false;
      offerBtn.disabled     = false;
      offerBtn.onclick = () => offerPeerConnection(ws, offerIdInput.value);
    })
    .catch((err) => console.error('Signaling connection failed:', err));

  // ---------------------------------------------------------------------------
  // Signaling: open WebSocket and route incoming messages to the right peer.
  // ---------------------------------------------------------------------------

  function openSignaling(url) {
    return new Promise((resolve, reject) => {
      const ws = new WebSocket(url);

      ws.onopen  = () => resolve(ws);
      ws.onerror = () => reject(new Error('WebSocket error'));
      ws.onclose = () => console.error('Signaling WebSocket disconnected');

      ws.onmessage = (e) => {
        if (typeof e.data !== 'string') return;

        const message = JSON.parse(e.data);
        const { id, type } = message; // id = sender's peer ID (set by server)

        console.log(`Signaling recv [${id}]:`, message);

        let pc = peerConnectionMap[id];

        if (!pc) {
          // No existing connection for this peer.
          // Only an 'offer' can initiate a new one; anything else is stale.
          if (type !== 'offer') {
            console.warn(`Unexpected message type "${type}" from [${id}] with no existing connection`);
            return;
          }

          // ANSWERER: create a PeerConnection in response to the incoming offer.
          console.log(`Answering offer from [${id}]`);
          pc = createPeerConnection(ws, id);
        }

        switch (type) {
          case 'offer':
          case 'answer':
            // Apply the remote SDP description, then reply with an answer if
            // we received an offer (the offerer side just applies and finishes).
            pc.setRemoteDescription({ sdp: message.description, type: message.type })
              .then(() => {
                if (type === 'offer') {
                  sendLocalDescription(ws, id, pc, 'answer');
                }
              });
            break;

          case 'candidate':
            // Trickle ICE: add each candidate as it arrives so connectivity
            // checks can start before all candidates have been gathered.
            pc.addIceCandidate({ candidate: message.candidate, sdpMid: message.mid });
            break;
        }
      };
    });
  }

  // ---------------------------------------------------------------------------
  // OFFERER: initiate a connection to a remote peer.
  // ---------------------------------------------------------------------------

  function offerPeerConnection(ws, id) {
    console.log(`Offering to [${id}]`);
    const pc = createPeerConnection(ws, id);

    // The offerer must create the DataChannel before sending the offer so the
    // channel description is embedded in the SDP.  The answerer will receive
    // the channel via ondatachannel (set up inside createPeerConnection).
    const dc = pc.createDataChannel('test');
    console.log(`DataChannel "test" created`);
    setupDataChannel(dc, id);

    // Kick off SDP negotiation.
    sendLocalDescription(ws, id, pc, 'offer');
  }

  // ---------------------------------------------------------------------------
  // PeerConnection: create, wire up ICE and DataChannel events.
  // ---------------------------------------------------------------------------

  function createPeerConnection(ws, id) {
    const pc = new RTCPeerConnection(config);

    pc.oniceconnectionstatechange = () =>
      console.log(`[${id}] ICE connection state: ${pc.iceConnectionState}`);

    pc.onicegatheringstatechange = () =>
      console.log(`[${id}] ICE gathering state: ${pc.iceGatheringState}`);

    // Trickle ICE: send each local candidate to the remote peer immediately
    // instead of waiting for all candidates to be gathered, so the remote
    // side can start connectivity checks sooner.
    pc.onicecandidate = (e) => {
      if (e.candidate?.candidate) {
        sendLocalCandidate(ws, id, e.candidate);
      }
    };

    // ANSWERER receives the DataChannel created by the offerer here.
    pc.ondatachannel = (e) => {
      const dc = e.channel;
      console.log(`[${id}] DataChannel "${dc.label}" received`);
      setupDataChannel(dc, id);

      // Greet the offerer as soon as the channel arrives on the answerer side.
      dc.send(`Hello from ${localId}`);
    };

    peerConnectionMap[id] = pc;
    return pc;
  }

  // ---------------------------------------------------------------------------
  // DataChannel: attach event handlers for open / close / message.
  // ---------------------------------------------------------------------------

  function setupDataChannel(dc, id) {
    // Channel becomes open once ICE succeeds and DTLS handshake completes.
    dc.onopen = () => {
      console.log(`[${id}] DataChannel "${dc.label}" open`);

      // Unlock the send UI and route the send button to this channel.
      sendMsgInput.disabled = false;
      sendBtn.disabled      = false;
      sendBtn.onclick = () => dc.send(sendMsgInput.value);
    };

    dc.onclose = () => console.log(`[${id}] DataChannel "${dc.label}" closed`);

    // Incoming P2P messages — no server is involved at this point.
    dc.onmessage = (e) => {
      if (typeof e.data !== 'string') return;
      console.log(`[${id}] message: ${e.data}`);
      document.body.appendChild(document.createTextNode(e.data));
    };

    dataChannelMap[id] = dc;
    return dc;
  }

  // ---------------------------------------------------------------------------
  // Signaling helpers: send SDP and ICE candidates through the WebSocket.
  // ---------------------------------------------------------------------------

  // Create a local SDP offer or answer, set it as the local description, then
  // forward it to the remote peer via the signaling channel.
  function sendLocalDescription(ws, id, pc, type) {
    (type === 'offer' ? pc.createOffer() : pc.createAnswer())
      .then((desc) => pc.setLocalDescription(desc))
      .then(() => {
        const { sdp, type } = pc.localDescription;
        ws.send(JSON.stringify({ id, type, description: sdp }));
      });
  }

  // Forward a locally discovered ICE candidate to the remote peer.
  function sendLocalCandidate(ws, id, cand) {
    const { candidate, sdpMid } = cand;
    ws.send(JSON.stringify({ id, type: 'candidate', candidate, mid: sdpMid }));
  }

  // ---------------------------------------------------------------------------
  // Utility
  // ---------------------------------------------------------------------------

  function randomId(length) {
    const chars = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz';
    return Array.from({ length }, () => chars[Math.floor(Math.random() * chars.length)]).join('');
  }
});
