#pragma once
#include "Epoch.h"
#include "Probe.h"
#include "Types.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>

namespace SynTm
{
    /// Fixed-size header prepended to sync messages.
    ///
    /// Layout (39 bytes):
    ///   [0]     version (1 byte, currently 1)
    ///   [1..8]  epoch id (8 bytes LE)
    ///   [9..16] epoch baseTime (8 bytes LE)
    ///   [17..24] epoch createdAt (8 bytes LE)
    ///   [25..28] epoch memberCount (4 bytes LE)
    ///   [29]    reserved (1 byte, 0)
    ///   [30..37] epoch offset (8 bytes LE, sender's SyncedNow - LocalNow)
    ///   [38..N] SyncPulse payload:
    ///             8 bytes  (ShortWireSize) → t1 only, no echo
    ///            24 bytes  (FullWireSize)  → t1 + echo_t1 + echo_t2
    ///
    /// Total: 46 bytes (short) or 62 bytes (full)
    struct SyncHeader
    {
        static constexpr std::uint8_t CurrentVersion = 1;
        static constexpr std::size_t MetaSize = 38; // Up to and including epoch offset.

        std::uint8_t version = CurrentVersion;
        EpochInfo epoch;
    };

    /// Write the 31-byte sync header into buf starting at offset 0.
    inline void WriteHeader(
        std::span<std::byte> buf,
        const EpochInfo& epoch) noexcept
    {
        buf[0] = static_cast<std::byte>(SyncHeader::CurrentVersion);
        Detail::WriteRawU64(buf, 1, epoch.epochId);
        Detail::WriteLE64(buf, 9, epoch.baseTime);
        Detail::WriteLE64(buf, 17, epoch.createdAt);
        std::uint32_t mc = epoch.memberCount;
        std::memcpy(buf.data() + 25, &mc, sizeof(mc));
        buf[29] = std::byte{0}; // Reserved.
        Detail::WriteLE64(buf, 30, epoch.epochOffset);
    }

    /// Serialize a sync header + SyncPulse into a buffer.
    /// Returns the number of bytes written, or 0 if the buffer is too small.
    [[nodiscard]] inline std::size_t WriteSyncPulse(
        std::span<std::byte> buf,
        const EpochInfo& epoch,
        const SyncPulse& pulse) noexcept
    {
        const std::size_t payloadSize =
            pulse.HasEcho() ? SyncPulse::FullWireSize : SyncPulse::ShortWireSize;
        const std::size_t totalSize = SyncHeader::MetaSize + payloadSize;
        if (buf.size() < totalSize) {
            return 0;
        }
        WriteHeader(buf, epoch);
        (void)WriteTo(buf.subspan(SyncHeader::MetaSize), pulse);
        return totalSize;
    }

    /// Result of parsing an incoming sync message.
    struct ParsedSyncMessage
    {
        EpochInfo epoch;
        std::optional<SyncPulse> pulse;
    };

    /// Parse a sync message from raw bytes.
    [[nodiscard]] inline std::optional<ParsedSyncMessage> ParseSyncMessage(
        std::span<const std::byte> buf) noexcept
    {
        if (buf.size() < SyncHeader::MetaSize) {
            return std::nullopt;
        }

        auto version = static_cast<std::uint8_t>(buf[0]);
        if (version != SyncHeader::CurrentVersion) {
            return std::nullopt;
        }

        EpochInfo epoch{
            .epochId     = Detail::ReadRawU64(buf, 1),
            .baseTime    = Detail::ReadLE64(buf, 9),
            .createdAt   = Detail::ReadLE64(buf, 17),
            .memberCount = 0,
            .epochOffset = Detail::ReadLE64(buf, 30),
        };
        std::uint32_t mc = 0;
        std::memcpy(&mc, buf.data() + 25, sizeof(mc));
        epoch.memberCount = mc;

        auto payload = buf.subspan(SyncHeader::MetaSize);

        ParsedSyncMessage result{.epoch = epoch};
        result.pulse = ReadSyncPulse(payload);
        if (!result.pulse) {
            return std::nullopt;
        }

        return result;
    }
}
