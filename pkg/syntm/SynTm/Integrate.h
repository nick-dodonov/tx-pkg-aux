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
    /// Message type byte for the sync layer header.
    enum class SyncMessageType : std::uint8_t
    {
        SyncPulse = 3, ///< Unified probe/reply (replaces old Request=1 and Response=2).
    };

    /// Fixed-size header prepended to sync messages.
    ///
    /// Layout (39 bytes):
    ///   [0]     version (1 byte, currently 1)
    ///   [1]     message type (1 byte)
    ///   [2..9]  epoch id (8 bytes LE)
    ///   [10..17] epoch baseTime (8 bytes LE)
    ///   [18..25] epoch createdAt (8 bytes LE)
    ///   [26..29] epoch memberCount (4 bytes LE)
    ///   [30]    reserved (1 byte, 0)
    ///   [31..38] epoch offset (8 bytes LE, sender's SyncedNow - LocalNow)
    ///   [39..N] SyncPulse payload:
    ///             8 bytes  (ShortWireSize) → t1 only, no echo
    ///            24 bytes  (FullWireSize)  → t1 + echo_t1 + echo_t2
    ///
    /// Total: 47 bytes (short) or 63 bytes (full)
    struct SyncHeader
    {
        static constexpr std::uint8_t CurrentVersion = 1;
        static constexpr std::size_t MetaSize = 39; // Up to and including epoch offset.

        std::uint8_t version = CurrentVersion;
        SyncMessageType type = SyncMessageType::SyncPulse;
        EpochInfo epoch;
    };

    /// Write the 31-byte sync header into buf starting at offset 0.
    inline void WriteHeader(
        std::span<std::byte> buf,
        SyncMessageType type,
        const EpochInfo& epoch) noexcept
    {
        buf[0] = static_cast<std::byte>(SyncHeader::CurrentVersion);
        buf[1] = static_cast<std::byte>(type);
        Detail::WriteRawU64(buf, 2, epoch.epochId);
        Detail::WriteLE64(buf, 10, epoch.baseTime);
        Detail::WriteLE64(buf, 18, epoch.createdAt);
        std::uint32_t mc = epoch.memberCount;
        std::memcpy(buf.data() + 26, &mc, sizeof(mc));
        buf[30] = std::byte{0}; // Reserved.
        Detail::WriteLE64(buf, 31, epoch.epochOffset);
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
        WriteHeader(buf, SyncMessageType::SyncPulse, epoch);
        (void)WriteTo(buf.subspan(SyncHeader::MetaSize), pulse);
        return totalSize;
    }

    /// Result of parsing an incoming sync message.
    struct ParsedSyncMessage
    {
        SyncMessageType type;
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

        auto type = static_cast<SyncMessageType>(buf[1]);
        EpochInfo epoch{
            .epochId     = Detail::ReadRawU64(buf, 2),
            .baseTime    = Detail::ReadLE64(buf, 10),
            .createdAt   = Detail::ReadLE64(buf, 18),
            .memberCount = 0,
            .epochOffset = Detail::ReadLE64(buf, 31),
        };
        std::uint32_t mc = 0;
        std::memcpy(&mc, buf.data() + 26, sizeof(mc));
        epoch.memberCount = mc;

        auto payload = buf.subspan(SyncHeader::MetaSize);

        ParsedSyncMessage result{.type = type, .epoch = epoch};

        switch (type)
        {
            case SyncMessageType::SyncPulse:
                result.pulse = ReadSyncPulse(payload);
                if (!result.pulse) {
                    return std::nullopt;
                }
                break;
            default:
                return std::nullopt;
        }

        return result;
    }
}
