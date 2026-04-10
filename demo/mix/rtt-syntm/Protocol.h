#pragma once
#include "SynTm/Types.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <vector>

namespace Demo
{
    /// Application-level message type tag (first byte of every message).
    enum class MsgType : std::uint8_t
    {
        SyncProbe   = 1, ///< SynTm probe (request or response via Integrate.h format).
        StateUpdate = 2, ///< Simulation state broadcast.
        ShotFired   = 3, ///< Coordinated shot announcement.
        ShotAck     = 4, ///< Shot receipt acknowledgment.
    };

    /// StateUpdate payload (POD).
    struct StateUpdateMsg
    {
        SynTm::Nanos syncTime; ///< Synced time when this update was created.
        float position;
        float velocity;
    };
    static_assert(std::is_trivially_copyable_v<StateUpdateMsg>);

    /// ShotFired payload (POD).
    struct ShotFiredMsg
    {
        SynTm::Nanos scheduledSyncTime; ///< The synced nanosecond at which the shot was fired.
    };
    static_assert(std::is_trivially_copyable_v<ShotFiredMsg>);

    /// ShotAck payload (POD).
    struct ShotAckMsg
    {
        SynTm::Nanos scheduledSyncTime; ///< The originally scheduled shot time.
        SynTm::Nanos receivedSyncTime;  ///< Synced time on the receiver when the shot was observed.
    };
    static_assert(std::is_trivially_copyable_v<ShotAckMsg>);

    // -----------------------------------------------------------------------
    // Serialization helpers
    // -----------------------------------------------------------------------

    /// Wrap a SynTm sync probe (already serialized via Integrate.h) with the app message tag.
    [[nodiscard]] inline std::vector<std::byte> WrapSyncProbe(std::span<const std::byte> probeBytes)
    {
        std::vector<std::byte> buf(1 + probeBytes.size());
        buf[0] = static_cast<std::byte>(MsgType::SyncProbe);
        std::memcpy(buf.data() + 1, probeBytes.data(), probeBytes.size());
        return buf;
    }

    /// Serialize a StateUpdate message.
    [[nodiscard]] inline std::vector<std::byte> SerializeStateUpdate(const StateUpdateMsg& msg)
    {
        std::vector<std::byte> buf(1 + sizeof(msg));
        buf[0] = static_cast<std::byte>(MsgType::StateUpdate);
        std::memcpy(buf.data() + 1, &msg, sizeof(msg));
        return buf;
    }

    /// Serialize a ShotFired message.
    [[nodiscard]] inline std::vector<std::byte> SerializeShotFired(const ShotFiredMsg& msg)
    {
        std::vector<std::byte> buf(1 + sizeof(msg));
        buf[0] = static_cast<std::byte>(MsgType::ShotFired);
        std::memcpy(buf.data() + 1, &msg, sizeof(msg));
        return buf;
    }

    /// Serialize a ShotAck message.
    [[nodiscard]] inline std::vector<std::byte> SerializeShotAck(const ShotAckMsg& msg)
    {
        std::vector<std::byte> buf(1 + sizeof(msg));
        buf[0] = static_cast<std::byte>(MsgType::ShotAck);
        std::memcpy(buf.data() + 1, &msg, sizeof(msg));
        return buf;
    }

    /// Parsed application message (returned by ParseAppMessage).
    struct ParsedAppMessage
    {
        MsgType type;
        std::span<const std::byte> payload; ///< Points into the original buffer (no copy).
    };

    /// Parse the type tag from a raw message buffer.
    [[nodiscard]] inline std::optional<ParsedAppMessage> ParseAppMessage(std::span<const std::byte> buf)
    {
        if (buf.empty()) {
            return std::nullopt;
        }
        auto type = static_cast<MsgType>(buf[0]);
        return ParsedAppMessage{.type = type, .payload = buf.subspan(1)};
    }

    /// Extract a StateUpdate from the payload portion.
    [[nodiscard]] inline std::optional<StateUpdateMsg> ReadStateUpdate(std::span<const std::byte> payload)
    {
        if (payload.size() < sizeof(StateUpdateMsg)) {
            return std::nullopt;
        }
        StateUpdateMsg msg;
        std::memcpy(&msg, payload.data(), sizeof(msg));
        return msg;
    }

    /// Extract a ShotFired from the payload portion.
    [[nodiscard]] inline std::optional<ShotFiredMsg> ReadShotFired(std::span<const std::byte> payload)
    {
        if (payload.size() < sizeof(ShotFiredMsg)) {
            return std::nullopt;
        }
        ShotFiredMsg msg;
        std::memcpy(&msg, payload.data(), sizeof(msg));
        return msg;
    }

    /// Extract a ShotAck from the payload portion.
    [[nodiscard]] inline std::optional<ShotAckMsg> ReadShotAck(std::span<const std::byte> payload)
    {
        if (payload.size() < sizeof(ShotAckMsg)) {
            return std::nullopt;
        }
        ShotAckMsg msg;
        std::memcpy(&msg, payload.data(), sizeof(msg));
        return msg;
    }
}
