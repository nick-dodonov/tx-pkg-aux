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
    // Unified sync probe (NTP-style four-timestamp model, 2-message cycle)
    // -----------------------------------------------------------------------

    /// Unified sync probe.
    ///
    /// Each SyncPulse simultaneously acts as a new probe and as a reply to the
    /// peer's last pulse.  echo_t3 is implicit: it equals t1 of this packet.
    ///
    /// NTP mapping when processing an incoming pulse:
    ///   t1 = echo_t1  (our previous send time, echoed back by peer)
    ///   t2 = echo_t2  (peer's receive time of our previous pulse)
    ///   t3 = pulse.t1 (peer's send time of this pulse, the implicit echo_t3)
    ///   t4 = receivedAt (our receive time of this pulse)
    struct SyncPulse
    {
        Ticks t1{};                   ///< Local send timestamp.
        std::optional<Ticks> echo_t1; ///< Peer's t1 from their last SyncPulse.
        std::optional<Ticks> echo_t2; ///< Time I received that SyncPulse (= t2).

        /// True when this pulse carries echo data needed for offset computation.
        [[nodiscard]] bool HasEcho() const noexcept
        {
            return echo_t1.has_value() && echo_t2.has_value();
        }

        static constexpr std::size_t ShortWireSize = sizeof(Ticks);        // 8 bytes
        static constexpr std::size_t FullWireSize  = 3 * sizeof(Ticks);    // 24 bytes
    };

    /// Computed from the four timestamps (t1..t4) after receiving a SyncPulse.
    struct ProbeResult
    {
        Ticks offset{}; ///< Estimated clock offset: remote - local.
        Ticks rtt{};    ///< Round-trip time.
    };

    /// Compute the probe result from the four NTP-style timestamps.
    ///   offset = ((t2 - t1) + (t3 - t4)) / 2
    ///   rtt    = (t4 - t1) - (t3 - t2)
    [[nodiscard]] constexpr ProbeResult ComputeProbeResult(
        Ticks t1, Ticks t2, Ticks t3, Ticks t4) noexcept
    {
        Ticks rtt    = (t4 - t1) - (t3 - t2);
        Ticks offset = ((t2 - t1) + (t3 - t4)) / 2;
        return {.offset = offset, .rtt = rtt};
    }

    // -----------------------------------------------------------------------
    // Serialization — fixed-size, little-endian, zero-allocation
    // -----------------------------------------------------------------------

    namespace Detail
    {
        inline void WriteLE64(std::span<std::byte> buf, std::size_t pos, Ticks value) noexcept
        {
            auto u = static_cast<std::uint64_t>(value.count());
            if constexpr (std::endian::native != std::endian::little) {
                u = std::byteswap(u);
            }
            std::memcpy(buf.data() + pos, &u, sizeof(u));
        }

        inline void WriteRawU64(std::span<std::byte> buf, std::size_t pos, std::uint64_t value) noexcept
        {
            auto u = value;
            if constexpr (std::endian::native != std::endian::little) {
                u = std::byteswap(u);
            }
            std::memcpy(buf.data() + pos, &u, sizeof(u));
        }

        [[nodiscard]] inline Ticks ReadLE64(std::span<const std::byte> buf, std::size_t pos) noexcept
        {
            std::uint64_t u = 0;
            std::memcpy(&u, buf.data() + pos, sizeof(u));
            if constexpr (std::endian::native != std::endian::little) {
                u = std::byteswap(u);
            }
            return Ticks{static_cast<std::int64_t>(u)};
        }

        [[nodiscard]] inline std::uint64_t ReadRawU64(std::span<const std::byte> buf, std::size_t pos) noexcept
        {
            std::uint64_t u = 0;
            std::memcpy(&u, buf.data() + pos, sizeof(u));
            if constexpr (std::endian::native != std::endian::little) {
                u = std::byteswap(u);
            }
            return u;
        }
    }

    /// Serialize a SyncPulse into the buffer.
    ///
    /// Writes SyncPulse::ShortWireSize (8) bytes when pulse.HasEcho() is false,
    /// or SyncPulse::FullWireSize (24) bytes when pulse.HasEcho() is true.
    /// Returns the number of bytes written, or 0 if the buffer is too small.
    [[nodiscard]] inline std::size_t WriteTo(
        std::span<std::byte> buf, const SyncPulse& pulse) noexcept
    {
        if (pulse.HasEcho()) {
            if (buf.size() < SyncPulse::FullWireSize) {
                return 0;
            }
            Detail::WriteLE64(buf, 0, pulse.t1);
            Detail::WriteLE64(buf, 8, *pulse.echo_t1);
            Detail::WriteLE64(buf, 16, *pulse.echo_t2);
            return SyncPulse::FullWireSize;
        }
        if (buf.size() < SyncPulse::ShortWireSize) {
            return 0;
        }
        Detail::WriteLE64(buf, 0, pulse.t1);
        return SyncPulse::ShortWireSize;
    }

    /// Deserialize a SyncPulse from the buffer.
    ///
    /// Payload of 8 bytes → no echo (short form).
    /// Payload of 24 bytes → full form with echo_t1 and echo_t2.
    [[nodiscard]] inline std::optional<SyncPulse> ReadSyncPulse(
        std::span<const std::byte> buf) noexcept
    {
        if (buf.size() == SyncPulse::FullWireSize) {
            return SyncPulse{
                .t1     = Detail::ReadLE64(buf, 0),
                .echo_t1 = Detail::ReadLE64(buf, 8),
                .echo_t2 = Detail::ReadLE64(buf, 16),
            };
        }
        if (buf.size() >= SyncPulse::ShortWireSize) {
            return SyncPulse{.t1 = Detail::ReadLE64(buf, 0)};
        }
        return std::nullopt;
    }
}
