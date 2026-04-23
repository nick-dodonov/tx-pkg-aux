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
    ///   - Now() reads an atomic snapshot — safe from any thread.
    ///   - Update() must be called from a single thread (the event loop).
    ///   - All other methods delegate to Consensus and share its threading model.
    class SyncClock final : public IClock
    {
    public:
        explicit SyncClock(Consensus& consensus)
            : _consensus(consensus)
        {
        }

        /// Call periodically from the event loop to refresh the atomic snapshot.
        void Update()
        {
            _cachedNow.store(_consensus.SyncedNow(), std::memory_order_relaxed);
        }

        /// Synchronized time in ticks.
        /// Thread-safe (reads atomic snapshot).
        [[nodiscard]] Ticks Now() const noexcept override
        {
            return _cachedNow.load(std::memory_order_relaxed);
        }

        /// Truncate the current synchronized time using the given TruncTime type.
        template <typename TruncTimeT>
        [[nodiscard]] TruncTimeT Truncate() const noexcept
        {
            return SynTm::Truncate<TruncTimeT::BitWidth, TruncTimeT::QuantumNs>(
                Now(), _consensus.Epoch().baseTime);
        }

        /// Expand a truncated time to an absolute tick value.
        template <typename TruncTimeT>
        [[nodiscard]] Ticks Expand(TruncTimeT trunc) const noexcept
        {
            return SynTm::Expand(trunc, _consensus.Epoch().baseTime, Now());
        }

        /// Expand a truncated time to a steady_clock time_point.
        template <typename TruncTimeT>
        [[nodiscard]] std::chrono::steady_clock::time_point ExpandToTimePoint(
            TruncTimeT trunc) const noexcept
        {
            auto ns = Expand(trunc);
            return std::chrono::steady_clock::time_point{ns};
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
        std::atomic<Ticks> _cachedNow{Ticks{}};
    };
}
