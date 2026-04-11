# SynTm — Distributed Time Synchronization

Transport-agnostic time synchronization subsystem providing consensus-based synchronized steady clock across linked nodes. All hot-path computations use integer arithmetic (`int64_t` nanoseconds); no floating-point in production code.

## Architecture

Six layers, bottom-up:

```
┌──────────────────────────────────────────────────────┐
│  SyncClock          User-facing chrono-compatible API│
├──────────────────────────────────────────────────────┤
│  Integrate          Wire-format serialization layer  │
├──────────────────────────────────────────────────────┤
│  Consensus          Multi-link epoch agreement       │
├──────────────────────────────────────────────────────┤
│  Session            Per-link state machine           │
├───────────────────┬──────────────────────────────────┤
│  Filter           │  DriftModel                      │
│  Offset/drift     │  Linear drift compensation       │
│  estimation       │  with step/slew steering         │
├───────────────────┴──────────────────────────────────┤
│  Clock · Probe · TruncTime · Types                   │
│  Core primitives — no external dependencies          │
└──────────────────────────────────────────────────────┘
```

## Key Design Decisions

- **Integer arithmetic**: `Nanos = int64_t` nanoseconds. Drift rate modeled as `Rational{num, den}` with `__int128` to prevent overflow.
- **Transport-agnostic**: Provides serialization helpers (`Integrate.h`) for embedding sync data into existing message framing via `std::span<std::byte>`. Does NOT own transport — caller drives probe exchange.
- **Truncated time**: `TruncTime<Bits, Quantum>` template for compact representations. E.g., `TruncTime16_1ms` = 16-bit index covering ~65s range.
- **Consensus via epoch propagation**: Connected components share a `SyncEpoch` (id + base time + rate). When groups merge, the older/larger epoch wins. Losers receive an `EpochChanged` event.
- **Viewer mode**: Passive nodes accept time from peers without voting — useful for spectators/monitors.

## File Reference

| File | Purpose |
|------|---------|
| `SynTm/Types.h` | `Nanos`, `Rational`, `SyncQuality`, `SyncEvent` |
| `SynTm/Clock.h` | `IClock` interface, `SteadyClock`, `FakeClock` |
| `SynTm/Probe.h` | NTP-style probe request/response, 4-timestamp computation |
| `SynTm/TruncTime.h` | Compact truncated time template with wrap-around resolution |
| `SynTm/Filter.h` | Sliding-window offset/drift estimation (weighted median + regression) |
| `SynTm/DriftModel.h` | Linear drift compensation with step/slew steering |
| `SynTm/SessionConfig.h` | Tunable parameters, `LanSessionConfig()`, `WanSessionConfig()` |
| `SynTm/Session.h` | Per-link state machine (Idle → Probing → Synced → Resyncing) |
| `SynTm/Epoch.h` | Epoch definition, strength comparison, wire format |
| `SynTm/Consensus.h` | Multi-link consensus manager, epoch propagation |
| `SynTm/SyncClock.h` | Thread-safe user-facing API, chrono integration |
| `SynTm/Integrate.h` | Wire-format serialization helpers for transport embedding |

## Integration Guide

### 1. Create nodes and connect peers

```cpp
#include "SynTm/Clock.h"
#include "SynTm/Consensus.h"
#include "SynTm/SyncClock.h"

SynTm::SteadyClock clock;
SynTm::Consensus consensus(clock, SynTm::ConsensusMode::Voter);
SynTm::SyncClock syncClock(consensus);

consensus.AddPeer("remote-node-id");
```

### 2. Exchange probes (caller-driven)

The subsystem does NOT own transport. Drive probe exchange from your network loop:

```cpp
// Periodically check if probing is needed.
if (consensus.ShouldProbe("peer-id")) {
    auto req = consensus.MakeProbeRequest("peer-id");
    // Serialize with Integrate.h and send over your transport.
}

// When a probe request arrives from the network:
auto resp = consensus.HandleProbeRequest("peer-id", incomingRequest);
// Send response back.

// When a probe response arrives:
consensus.HandleProbeResponse("peer-id", incomingResponse, remoteEpochInfo);
```

### 3. Read synchronized time

```cpp
syncClock.Update(); // Call periodically from your main loop.

auto now = syncClock.Now();           // std::chrono::steady_clock::time_point
auto ns  = syncClock.NowNanos();      // Raw int64_t nanoseconds
bool ok  = syncClock.IsSynced();      // Have we converged?
auto q   = syncClock.Quality();       // SyncQuality::None/Low/High
```

### 4. Use truncated time for compact transmission

```cpp
// Sender
auto trunc = syncClock.Truncate<SynTm::TruncTime16_1ms>();
// Send trunc.index (2 bytes) over the wire.

// Receiver
SynTm::Nanos absolute = syncClock.Expand(trunc);
```

### 5. Wire format (Integrate.h)

Embed sync data in your existing message framing:

```cpp
#include "SynTm/Integrate.h"

std::array<std::byte, 128> buf;

// Write a probe request with epoch header.
auto bytes = SynTm::WriteSyncProbeRequest(buf, consensus.OurEpochInfo(), req);
// Send buf[0..bytes) over your transport.

// Parse incoming message.
auto parsed = SynTm::ParseSyncMessage(receivedSpan);
if (parsed && parsed->request) {
    consensus.HandleRemoteEpoch(parsed->epoch);
    auto resp = consensus.HandleProbeRequest("peer", *parsed->request);
}
```

### 6. Listen for events

```cpp
syncClock.OnEvent([](SynTm::SyncEvent e) {
    switch (e) {
        case SynTm::SyncEvent::SyncAcquired:    /* first convergence */  break;
        case SynTm::SyncEvent::EpochChanged:    /* group merge */        break;
        case SynTm::SyncEvent::ResyncStarted:   /* time jumped */        break;
        case SynTm::SyncEvent::ResyncCompleted: /* stable again */       break;
        case SynTm::SyncEvent::SyncLost:        /* all peers gone */     break;
    }
});
```

## Build

```bash
# Build the library
bazel build //pkg/syntm

# Run tests (85 tests across 15 suites)
bazel test //test/pkg/syntm:syntm
```

## Known Limitations

- **No midpoint consensus**: Each node corrects toward its best peer rather than computing a group midpoint. Suitable for client-server and star topologies; symmetric peer-to-peer use cases see each node converging to its peer's timeline.
- **Single-threaded consensus**: `Consensus` is not thread-safe; serialize all calls from one thread/event loop. `SyncClock` reads are thread-safe (atomic cache).
- **No persistent state**: Sync state is lost on restart. Nodes must re-probe to converge.
- **Fixed probe scheduling**: Adaptive interval scales linearly between min and max. No exponential backoff or jitter.
