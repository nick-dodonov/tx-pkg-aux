#pragma once
#include "Filter.h"
#include "Types.h"

#include "Log/Log.h"
#include "Log/Sep.h"

namespace SynTm
{
    /// Policy controlling how the DriftModel steers corrections.
    struct SteerPolicy
    {
        /// If the absolute correction exceeds this threshold (nanos),
        /// apply a step correction instead of slewing.
        /// Default: 100ms — corrections larger than this cause a time jump.
        Nanos stepThreshold = 100'000'000;

        /// Maximum slew rate: how fast to adjust (nanos per nanos of local time).
        /// Expressed as Rational. Default: 500ppm — ±0.5ms per second.
        Rational maxSlewRate{.num=500, .den=1'000'000};
    };

    /// Tracks the mapping from local time to synchronized time,
    /// compensating for measured offset and drift.
    ///
    /// The model: syncedTime = (localTime - baseLocal) * rate.num / rate.den + baseSynced
    ///
    /// Pure computation — no I/O, no timers, fully testable.
    class DriftModel
    {
    public:
        explicit DriftModel(SteerPolicy policy = {}) : _policy(policy) {}

        /// Initialize the model at a given local time with a known offset.
        /// Called once when the first filter result arrives.
        void Initialize(Nanos localTime, Nanos offset)
        {
            _baseLocal = localTime;
            _baseSynced = localTime + offset;
            _rate = Rational{.num=1, .den=1};
            _initialized = true;
            Log::Trace("baseLocal={} baseSynced={} offset={}ns",
                Log::Sep{_baseLocal}, Log::Sep{_baseSynced}, Log::Sep{offset});
        }

        /// Apply a filter result to steer the model.
        /// Returns true if a step correction was applied (re-sync event).
        bool Steer(Nanos localTime, FilterResult result)
        {
            if (!_initialized) {
                Initialize(localTime, result.offset);
                return false;
            }

            // Current synced time estimate at this local time.
            Nanos currentSynced = Convert(localTime);
            // Target synced time from the filter.
            Nanos targetSynced = localTime + result.offset;
            Nanos correction = targetSynced - currentSynced;

            // Check step occurrence based on absolute correction magnitude.
            Log::Trace("correction={}ns threshold={}ns currentSynced={} targetSynced={}",
                Log::Sep{correction}, Log::Sep{_policy.stepThreshold}, Log::Sep{currentSynced}, Log::Sep{targetSynced});

            Nanos absCorrectionNs = correction < 0 ? -correction : correction;
            if (absCorrectionNs > _policy.stepThreshold) {
                // Step: jump directly.
                _baseLocal = localTime;
                _baseSynced = targetSynced;
                _rate = result.rate;
                Log::Trace("STEP baseLocal={} baseSynced={} rate={}/{}({})",
                    Log::Sep{_baseLocal}, Log::Sep{_baseSynced}, Log::Sep{_rate.num}, Log::Sep{_rate.den}, _rate.ToDouble());
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
            Nanos slewAmount = correction / 2;

            // Re-base to current time.
            _baseSynced = currentSynced + slewAmount;
            _baseLocal = localTime;

            Log::Trace("slew slewAmount={}ns newBaseSynced={} rate={}/{}({})",
                Log::Sep{slewAmount}, Log::Sep{_baseSynced}, Log::Sep{_rate.num}, Log::Sep{_rate.den}, _rate.ToDouble());

            return false; // Smooth correction.
        }

        /// Convert a local time to synchronized time.
        /// Before initialization the default field values (_baseLocal=0,
        /// _baseSynced=0, _rate=1/1) produce passthrough: Convert(t) == t.
        [[nodiscard]] Nanos Convert(Nanos localTime) const noexcept
        {
            Nanos elapsed = localTime - _baseLocal;
            Nanos scaledElapsed = _rate.Apply(elapsed);
            return _baseSynced + scaledElapsed;
        }

        /// Current rate.
        [[nodiscard]] Rational Rate() const noexcept { return _rate; }

        /// Whether the model has been initialized.
        [[nodiscard]] bool IsInitialized() const noexcept { return _initialized; }

        /// Reset the model. Used on epoch change.
        void Reset() noexcept
        {
            _initialized = false;
            _baseLocal = 0;
            _baseSynced = 0;
            _rate = Rational{.num=1, .den=1};
        }

        /// Read current policy.
        [[nodiscard]] const SteerPolicy& Policy() const noexcept { return _policy; }

    private:
        SteerPolicy _policy;
        bool _initialized = false;
        Nanos _baseLocal = 0;
        Nanos _baseSynced = 0;
        Rational _rate{.num=1, .den=1};
    };
}
