// WebRTC polyfill for Node.js test environments.
//
// This file is injected before the Emscripten WASM module (--extern-pre-js) in
// WASM test builds, so RTCPeerConnection and friends are available globally when
// Emscripten initialises JsRtcTransport.
//
// In browser builds this file is not included (multi_test only adds it under
// the wasm32 platform select).  In a real browser RTCPeerConnection is always
// natively available, so this code would be a no-op anyway.
//
// Requires: node-datachannel npm package installed globally.
//   npm install -g node-datachannel

if (typeof globalThis.RTCPeerConnection === 'undefined' && typeof require !== 'undefined') {
    try {
        // Derive the global node_modules directory from `npm root -g`.
        // This is more portable than hard-coding /usr/local/lib/node_modules because
        // the global prefix varies by platform and Node.js installation method.
        const { execFileSync } = require('child_process');
        const npmRoot = execFileSync('npm', ['root', '-g'], { encoding: 'utf8' }).trim();
        const polyfill = require(npmRoot + '/node-datachannel/dist/cjs/polyfill/index.cjs');

        if (polyfill && polyfill.RTCPeerConnection) {
            // In Node.js, RTCPeerConnection host ICE candidates use the system's LAN
            // interface by default, which cannot reach other RTCPeerConnections in the
            // same process.  Binding to loopback forces 127.0.0.1 candidates that work
            // for in-process WebRTC tests without requiring a STUN server.
            const OriginalRTCPeerConnection = polyfill.RTCPeerConnection;
            class RTCPeerConnection extends OriginalRTCPeerConnection {
                constructor(config) {
                    super({ ...config, bindAddress: '127.0.0.1' });
                }
            }
            globalThis.RTCPeerConnection = RTCPeerConnection;
            globalThis.RTCSessionDescription = polyfill.RTCSessionDescription;
            globalThis.RTCIceCandidate = polyfill.RTCIceCandidate;
        }
    } catch (e) {
        // Polyfill not available.  The test will fail at runtime when JsRtcTransport
        // tries to create an RTCPeerConnection, which gives a clear error message.
    }
}
