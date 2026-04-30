#pragma once
#include "Filter.h"
#include "Types.h"

#include "Log/Log.h"
#include "Log/Sep.h"

#include <chrono>

namespace SynTm
{
    using namespace std::chrono_literals;
    /// Policy controlling how the DriftModel steers corrections.
    struct SteerPolicy
    {
        /// If the absolute correction exceeds this threshold (ticks),
        /// apply a step correction instead of slewing.
        /// Default: 100ms — corrections larger than this cause a time jump.
        Ticks stepThreshold = 100ms;

        /// Maximum slew rate: how fast to adjust (ticks per tick of local time).
        /// Expressed as DriftRate (ppb). Default: 500'000 ppb = 500 ppm — ±0.5 ms per second.
        ///
        /// TODO: maxSlewRate is declared but unused — slew currently applies 50% of correction
        /// directly. Implement as: clamp computed slew to maxSlewRate * elapsed before applying.
        DriftRate maxSlewRate = 500us;

        /// Parent area logger.
        Log::Logger parentLogger;
    };

    /// Tracks the mapping from local time to synchronized time,
    /// compensating for measured offset and drift.
    ///
    /// The model: syncedTime = rate.Apply(localTime - baseLocal) + baseSynced
    ///
    /// Pure computation — no I/O, no timers, fully testable.
    class DriftModel
    {
    public:
        explicit DriftModel(SteerPolicy policy = {}) 
            : _logger("DriftModel", policy.parentLogger)
            , _policy(policy)
        {}

        /// Initialize the model at a given local time with a known offset.
        /// Called once when the first filter result arrives.
        void Initialize(Ticks localTime, Ticks offset)
        {
            _baseLocal = localTime;
            _baseSynced = localTime + offset;
            _rate = DriftRate{};
            _initialized = true;
            _logger.Trace("baseLocal={} baseSynced={} offset={}ns",
                Log::Sep{_baseLocal.count()}, Log::Sep{_baseSynced.count()}, Log::Sep{offset.count()});
        }

        /// Apply a filter result to steer the model.
        /// Returns true if a step correction was applied (re-sync event).
        bool Steer(Ticks localTime, FilterResult result)
        {
            if (!_initialized) {
                Initialize(localTime, result.offset);
                return false;
            }

            // Current synced time estimate at this local time.
            Ticks currentSynced = Convert(localTime);
            // Target synced time from the filter.
            Ticks targetSynced = localTime + result.offset;
            Ticks correction = targetSynced - currentSynced;
            _lastCorrection = correction;

            // Check step occurrence based on absolute correction magnitude.
            _logger.Trace("correction={}ns threshold={}ns currentSynced={} targetSynced={}",
                Log::Sep{correction.count()}, Log::Sep{_policy.stepThreshold.count()}, Log::Sep{currentSynced.count()}, Log::Sep{targetSynced.count()});

            auto absCorrection = std::chrono::abs(correction);
            if (absCorrection > _policy.stepThreshold) {
                // Step: jump directly.
                _baseLocal = localTime;
                _baseSynced = targetSynced;
                _rate = result.rate;
                ++_stepCount;
                _lastSlewAmount = correction; // Full correction on step.
                _logger.Trace("STEP baseLocal={} baseSynced={} rate={}ns/s ({:.6f})",
                    Log::Sep{_baseLocal.count()}, Log::Sep{_baseSynced.count()}, _rate.count(), _rate.ToDouble());
                return true; // Step occurred.
            }

            // Slew: adjust the base to incorporate the correction gradually,
            // limited by maxSlewRate to prevent noisy probes from causing
            // large instantaneous jumps in synced time.
            _rate = result.rate;

            // Compute the elapsed time since the last steer call to derive
            // the maximum allowed slew delta for this interval.
            // maxSlewRate is in ns/s; elapsed is in ns → maxDelta in ns.
            Ticks elapsed = localTime - _baseLocal;
            Ticks maxDelta{};
            if (elapsed > Ticks{} && _policy.maxSlewRate.count() > 0) {
                // maxDelta = maxSlewRate [ns/s] * elapsed [ns] / 1e9
                auto wide = static_cast<__int128>(_policy.maxSlewRate.count()) *
                            static_cast<__int128>(elapsed.count());
                maxDelta = Ticks{static_cast<std::int64_t>(
                    wide / static_cast<__int128>(DriftRate::period::den))};
            } else {
                // First call or zero elapsed: fall back to 50% of correction.
                maxDelta = std::chrono::abs(correction) / 2;
            }

            // Clamp correction to [-maxDelta, +maxDelta].
            Ticks slewAmount;
            if (correction > maxDelta) {
                slewAmount = maxDelta;
            } else if (correction < -maxDelta) {
                slewAmount = -maxDelta;
            } else {
                slewAmount = correction;
            }
            _lastSlewAmount = slewAmount;

            // Re-base to current time.
            _baseSynced = currentSynced + slewAmount;
            _baseLocal = localTime;

            _logger.Trace("slew slewAmount={}ns (max={}ns) newBaseSynced={} rate={}ns/s ({:.6f})",
                Log::Sep{slewAmount.count()}, Log::Sep{maxDelta.count()},
                Log::Sep{_baseSynced.count()}, _rate.count(), _rate.ToDouble());

            return false; // Smooth correction.
        }

        /// Convert a local time to synchronized time.
        /// Before initialization the default field values (_baseLocal=0,
        /// _baseSynced=0, _rate=1/1) produce passthrough: Convert(t) == t.
        [[nodiscard]] Ticks Convert(Ticks localTime) const noexcept
        {
            Ticks elapsed = localTime - _baseLocal;
            Ticks scaledElapsed = _rate.Apply(elapsed);
            return _baseSynced + scaledElapsed;
        }

        /// Current rate.
        [[nodiscard]] DriftRate Rate() const noexcept { return _rate; }

        /// Whether the model has been initialized.
        [[nodiscard]] bool IsInitialized() const noexcept { return _initialized; }

        /// Reset the model. Used on epoch change.
        void Reset() noexcept
        {
            _initialized = false;
            _baseLocal = {};
            _baseSynced = {};
            _rate = DriftRate{};
        }

        /// Read current policy.
        [[nodiscard]] const SteerPolicy& Policy() const noexcept { return _policy; }

        /// Number of step corrections applied since construction or last Reset().
        [[nodiscard]] std::uint32_t StepCount() const noexcept { return _stepCount; }

        /// Last correction computed (targetSynced - currentSynced).
        [[nodiscard]] Ticks LastCorrection() const noexcept { return _lastCorrection; }

        /// Last slew amount actually applied (clamped correction).
        [[nodiscard]] Ticks LastSlewAmount() const noexcept { return _lastSlewAmount; }

    private:
        Log::Logger _logger;
        SteerPolicy _policy;
        bool _initialized = false;
        Ticks _baseLocal{};
        Ticks _baseSynced{};
        DriftRate _rate{};
        std::uint32_t _stepCount = 0;
        Ticks _lastCorrection{};
        Ticks _lastSlewAmount{};
    };
}
