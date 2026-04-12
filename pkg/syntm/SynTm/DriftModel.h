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
        DriftRate maxSlewRate{.ppb = 500'000};
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
        explicit DriftModel(SteerPolicy policy = {}) : _policy(policy) {}

        /// Initialize the model at a given local time with a known offset.
        /// Called once when the first filter result arrives.
        void Initialize(Ticks localTime, Ticks offset)
        {
            _baseLocal = localTime;
            _baseSynced = localTime + offset;
            _rate = DriftRate{};
            _initialized = true;
            Log::Trace("baseLocal={} baseSynced={} offset={}ns",
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

            // Check step occurrence based on absolute correction magnitude.
            Log::Trace("correction={}ns threshold={}ns currentSynced={} targetSynced={}",
                Log::Sep{correction.count()}, Log::Sep{_policy.stepThreshold.count()}, Log::Sep{currentSynced.count()}, Log::Sep{targetSynced.count()});

            auto absCorrection = std::chrono::abs(correction);
            if (absCorrection > _policy.stepThreshold) {
                // Step: jump directly.
                _baseLocal = localTime;
                _baseSynced = targetSynced;
                _rate = result.rate;
                Log::Trace("STEP baseLocal={} baseSynced={} rate={}ppb ({:.6f})",
                    Log::Sep{_baseLocal.count()}, Log::Sep{_baseSynced.count()}, _rate.ppb, _rate.ToDouble());
                return true; // Step occurred.
            }

            // Slew: adjust the base to gradually incorporate the correction.
            // We blend the rate from the filter and add a slew component.
            _rate = result.rate;

            // Apply a fraction of the correction to baseSynced.
            // Slew factor: apply correction over time proportional
            // to the correction magnitude / max slew rate.
            // For simplicity, apply 50% of the correction immediately
            // to baseSynced and let the rate handle the rest.
            Ticks slewAmount = correction / 2;

            // Re-base to current time.
            _baseSynced = currentSynced + slewAmount;
            _baseLocal = localTime;

            Log::Trace("slew slewAmount={}ns newBaseSynced={} rate={}ppb ({:.6f})",
                Log::Sep{slewAmount.count()}, Log::Sep{_baseSynced.count()}, _rate.ppb, _rate.ToDouble());

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

    private:
        SteerPolicy _policy;
        bool _initialized = false;
        Ticks _baseLocal{};
        Ticks _baseSynced{};
        DriftRate _rate{};
    };
}
