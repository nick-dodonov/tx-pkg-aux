#pragma once
#include "Consensus.h"
#include "TruncTime.h"
#include "Types.h"

#include <atomic>
#include <chrono>

namespace SynTm
{
    /// User-facing synchronized clock API.
    ///
    /// Wraps a Consensus instance and exposes chrono-compatible time,
    /// truncated time, and sync state queries.
    ///
    /// Thread safety:
    ///   - Now() and NowNanos() read an atomic snapshot — safe from any thread.
    ///   - Update() must be called from a single thread (the event loop).
    ///   - All other methods delegate to Consensus and share its threading model.
    class SyncClock
    {
    public:
        explicit SyncClock(Consensus& consensus)
            : _consensus(consensus)
        {
        }

        /// Call periodically from the event loop to refresh the atomic snapshot.
        void Update()
        {
            _cachedSyncedNow.store(_consensus.SyncedNow(), std::memory_order_relaxed);
        }

        /// Synchronized time as a steady_clock time_point.
        /// Thread-safe (reads atomic snapshot).
        [[nodiscard]] std::chrono::steady_clock::time_point Now() const noexcept
        {
            auto ns = _cachedSyncedNow.load(std::memory_order_relaxed);
            return std::chrono::steady_clock::time_point{
                std::chrono::nanoseconds{ns}};
        }

        /// Synchronized time in raw nanoseconds.
        /// Thread-safe (reads atomic snapshot).
        [[nodiscard]] Nanos NowNanos() const noexcept
        {
            return _cachedSyncedNow.load(std::memory_order_relaxed);
        }

        /// Truncate the current synchronized time using the given TruncTime type.
        template <typename TruncTimeT>
        [[nodiscard]] TruncTimeT Truncate() const noexcept
        {
            return SynTm::Truncate<TruncTimeT::BitWidth, TruncTimeT::QuantumNs>(
                NowNanos(), _consensus.Epoch().baseTime);
        }

        /// Expand a truncated time to an absolute nanosecond value.
        template <typename TruncTimeT>
        [[nodiscard]] Nanos Expand(TruncTimeT trunc) const noexcept
        {
            return SynTm::Expand(trunc, _consensus.Epoch().baseTime, NowNanos());
        }

        /// Expand a truncated time to a steady_clock time_point.
        template <typename TruncTimeT>
        [[nodiscard]] std::chrono::steady_clock::time_point ExpandToTimePoint(
            TruncTimeT trunc) const noexcept
        {
            auto ns = Expand(trunc);
            return std::chrono::steady_clock::time_point{
                std::chrono::nanoseconds{ns}};
        }

        /// Whether the system is synchronized.
        [[nodiscard]] bool IsSynced() const noexcept { return _consensus.IsSynced(); }

        /// Current sync quality.
        [[nodiscard]] SyncQuality Quality() const noexcept { return _consensus.Quality(); }

        /// Register event callback (delegates to Consensus).
        void OnEvent(Consensus::EventCallback callback)
        {
            _consensus.OnEvent(std::move(callback));
        }

        /// Access the underlying consensus (for advanced use / testing).
        [[nodiscard]] Consensus& GetConsensus() noexcept { return _consensus; }
        [[nodiscard]] const Consensus& GetConsensus() const noexcept { return _consensus; }

    private:
        Consensus& _consensus;
        std::atomic<Nanos> _cachedSyncedNow{0};
    };
}
