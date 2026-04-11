#pragma once
#include "Consensus.h"
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
        ProbeRequest  = 1,
        ProbeResponse = 2,
    };

    /// Fixed-size header prepended to sync messages.
    ///
    /// Layout (41 bytes):
    ///   [0]     version (1 byte, currently 1)
    ///   [1]     message type (1 byte)
    ///   [2..9]  epoch id (8 bytes LE)
    ///   [10..17] epoch baseTime (8 bytes LE)
    ///   [18..25] epoch createdAt (8 bytes LE)
    ///   [26..29] epoch memberCount (4 bytes LE)
    ///   [30]    reserved (1 byte, 0)
    ///   [31..N] probe data (8 or 24 bytes depending on type)
    ///
    /// Total: 31 + ProbeRequest::WireSize = 39 bytes for request
    ///        31 + ProbeResponse::WireSize = 55 bytes for response
    struct SyncHeader
    {
        static constexpr std::uint8_t CurrentVersion = 1;
        static constexpr std::size_t MetaSize = 31; // Up to and including reserved byte.

        std::uint8_t version = CurrentVersion;
        SyncMessageType type = SyncMessageType::ProbeRequest;
        EpochInfo epoch;
    };

    /// Serialize a sync header + probe request into a buffer.
    /// Returns the number of bytes written, or 0 if the buffer is too small.
    [[nodiscard]] inline std::size_t WriteSyncProbeRequest(
        std::span<std::byte> buf,
        const EpochInfo& epoch,
        const ProbeRequest& req) noexcept
    {
        constexpr std::size_t totalSize = SyncHeader::MetaSize + ProbeRequest::WireSize;
        if (buf.size() < totalSize) {
            return 0;
        }

        buf[0] = static_cast<std::byte>(SyncHeader::CurrentVersion);
        buf[1] = static_cast<std::byte>(SyncMessageType::ProbeRequest);
        Detail::WriteLE64(buf, 2, static_cast<Ticks>(epoch.epochId));
        Detail::WriteLE64(buf, 10, epoch.baseTime);
        Detail::WriteLE64(buf, 18, epoch.createdAt);

        std::uint32_t mc = epoch.memberCount;
        std::memcpy(buf.data() + 26, &mc, sizeof(mc));
        buf[30] = std::byte{0}; // Reserved.

        (void)WriteTo(buf.subspan(SyncHeader::MetaSize), req);
        return totalSize;
    }

    /// Serialize a sync header + probe response into a buffer.
    [[nodiscard]] inline std::size_t WriteSyncProbeResponse(
        std::span<std::byte> buf,
        const EpochInfo& epoch,
        const ProbeResponse& resp) noexcept
    {
        constexpr std::size_t totalSize = SyncHeader::MetaSize + ProbeResponse::WireSize;
        if (buf.size() < totalSize) {
            return 0;
        }

        buf[0] = static_cast<std::byte>(SyncHeader::CurrentVersion);
        buf[1] = static_cast<std::byte>(SyncMessageType::ProbeResponse);
        Detail::WriteLE64(buf, 2, static_cast<Ticks>(epoch.epochId));
        Detail::WriteLE64(buf, 10, epoch.baseTime);
        Detail::WriteLE64(buf, 18, epoch.createdAt);

        std::uint32_t mc = epoch.memberCount;
        std::memcpy(buf.data() + 26, &mc, sizeof(mc));
        buf[30] = std::byte{0};

        (void)WriteTo(buf.subspan(SyncHeader::MetaSize), resp);
        return totalSize;
    }

    /// Result of parsing an incoming sync message.
    struct ParsedSyncMessage
    {
        SyncMessageType type;
        EpochInfo epoch;
        std::optional<ProbeRequest> request;
        std::optional<ProbeResponse> response;
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
            .epochId     = static_cast<std::uint64_t>(Detail::ReadLE64(buf, 2)),
            .baseTime    = Detail::ReadLE64(buf, 10),
            .createdAt   = Detail::ReadLE64(buf, 18),
            .memberCount = 0,
        };
        std::uint32_t mc = 0;
        std::memcpy(&mc, buf.data() + 26, sizeof(mc));
        epoch.memberCount = mc;

        auto payload = buf.subspan(SyncHeader::MetaSize);

        ParsedSyncMessage result{.type = type, .epoch = epoch};

        switch (type)
        {
            case SyncMessageType::ProbeRequest:
                result.request = ReadProbeRequest(payload);
                if (!result.request) {
                    return std::nullopt;
                }
                break;
            case SyncMessageType::ProbeResponse:
                result.response = ReadProbeResponse(payload);
                if (!result.response) {
                    return std::nullopt;
                }
                break;
            default:
                return std::nullopt;
        }

        return result;
    }
}
