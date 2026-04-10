#pragma once
#include "Types.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>

namespace SynTm
{
    // -----------------------------------------------------------------------
    // Wire-format probe structures (NTP-style four-timestamp model)
    // -----------------------------------------------------------------------

    /// Sent by the initiator to request a probe.
    /// Contains t1: the local time at which the request was sent.
    struct ProbeRequest
    {
        Nanos t1 = 0; ///< Origin timestamp (initiator's local time at send).

        static constexpr std::size_t WireSize = sizeof(Nanos);
    };

    /// Sent by the responder after receiving a ProbeRequest.
    /// Contains t1 (echoed), t2 (responder's receive time), t3 (responder's send time).
    struct ProbeResponse
    {
        Nanos t1 = 0; ///< Echoed origin timestamp from the request.
        Nanos t2 = 0; ///< Responder's local time at request reception.
        Nanos t3 = 0; ///< Responder's local time at response transmission.

        static constexpr std::size_t WireSize = 3 * sizeof(Nanos);
    };

    /// Computed from the four timestamps (t1..t4) after the initiator receives
    /// the response. t4 is the initiator's local time at response reception.
    struct ProbeResult
    {
        Nanos offset = 0; ///< Estimated clock offset: remote - local.
        Nanos rtt    = 0; ///< Round-trip time.
    };

    /// Compute the probe result from the four NTP-style timestamps.
    ///   offset = ((t2 - t1) + (t3 - t4)) / 2
    ///   rtt    = (t4 - t1) - (t3 - t2)
    [[nodiscard]] constexpr ProbeResult ComputeProbeResult(
        Nanos t1, Nanos t2, Nanos t3, Nanos t4) noexcept
    {
        Nanos rtt    = (t4 - t1) - (t3 - t2);
        Nanos offset = ((t2 - t1) + (t3 - t4)) / 2;
        return {.offset = offset, .rtt = rtt};
    }

    // -----------------------------------------------------------------------
    // Serialization — fixed-size, little-endian, zero-allocation
    // -----------------------------------------------------------------------

    namespace Detail
    {
        inline void WriteLE64(std::span<std::byte> buf, std::size_t pos, Nanos value) noexcept
        {
            auto u = static_cast<std::uint64_t>(value);
            if constexpr (std::endian::native != std::endian::little) {
                u = std::byteswap(u);
            }
            std::memcpy(buf.data() + pos, &u, sizeof(u));
        }

        [[nodiscard]] inline Nanos ReadLE64(std::span<const std::byte> buf, std::size_t pos) noexcept
        {
            std::uint64_t u = 0;
            std::memcpy(&u, buf.data() + pos, sizeof(u));
            if constexpr (std::endian::native != std::endian::little) {
                u = std::byteswap(u);
            }
            return static_cast<Nanos>(u);
        }
    }

    /// Serialize a ProbeRequest into the buffer.
    /// Returns the number of bytes written (always ProbeRequest::WireSize),
    /// or 0 if the buffer is too small.
    [[nodiscard]] inline std::size_t WriteTo(
        std::span<std::byte> buf, const ProbeRequest& req) noexcept
    {
        if (buf.size() < ProbeRequest::WireSize) {
            return 0;
        }
        Detail::WriteLE64(buf, 0, req.t1);
        return ProbeRequest::WireSize;
    }

    /// Deserialize a ProbeRequest from the buffer.
    [[nodiscard]] inline std::optional<ProbeRequest> ReadProbeRequest(
        std::span<const std::byte> buf) noexcept
    {
        if (buf.size() < ProbeRequest::WireSize) {
            return std::nullopt;
        }
        return ProbeRequest{.t1 = Detail::ReadLE64(buf, 0)};
    }

    /// Serialize a ProbeResponse into the buffer.
    [[nodiscard]] inline std::size_t WriteTo(
        std::span<std::byte> buf, const ProbeResponse& resp) noexcept
    {
        if (buf.size() < ProbeResponse::WireSize) {
            return 0;
        }
        Detail::WriteLE64(buf, 0, resp.t1);
        Detail::WriteLE64(buf, 8, resp.t2);
        Detail::WriteLE64(buf, 16, resp.t3);
        return ProbeResponse::WireSize;
    }

    /// Deserialize a ProbeResponse from the buffer.
    [[nodiscard]] inline std::optional<ProbeResponse> ReadProbeResponse(
        std::span<const std::byte> buf) noexcept
    {
        if (buf.size() < ProbeResponse::WireSize) {
            return std::nullopt;
        }
        return ProbeResponse{
            .t1 = Detail::ReadLE64(buf, 0),
            .t2 = Detail::ReadLE64(buf, 8),
            .t3 = Detail::ReadLE64(buf, 16),
        };
    }
}
