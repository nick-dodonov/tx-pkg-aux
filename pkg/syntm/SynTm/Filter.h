#pragma once
#include "Probe.h"
#include "Types.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace SynTm
{
    /// Result produced by the Filter after processing enough samples.
    struct FilterResult
    {
        Nanos offset = 0; ///< Best estimate of clock offset (remote - local).
        Rational rate{.num=1, .den=1}; ///< Estimated drift rate (local → remote).
        Nanos jitter = 0; ///< Dispersion measure (interquartile range of offsets).
        Nanos minRtt = 0; ///< Minimum RTT seen in the current window.
        std::size_t sampleCount = 0; ///< Number of samples in the window that produced this result.
    };

    /// Sliding-window filter that estimates clock offset and drift from
    /// a stream of ProbeResult samples.
    ///
    /// Offset estimation: weighted median — samples with lower RTT get
    /// higher weight (they have less noise from queuing/path asymmetry).
    ///
    /// Drift estimation: integer linear regression of offset vs. local time
    /// over the sample window, producing a Rational rate.
    ///
    /// All hot-path arithmetic is integer-only.
    class Filter
    {
    public:
        struct Sample
        {
            Nanos localTime = 0; ///< Local time when the sample was taken.
            Nanos offset = 0; ///< Measured offset from probe.
            Nanos rtt = 0; ///< Measured RTT from probe.
        };

        explicit Filter(std::size_t windowSize = 8) : _windowSize(windowSize)
        {
            _samples.reserve(windowSize);
        }

        /// Add a new probe result sample taken at the given local time.
        /// Always returns a FilterResult; inspect sampleCount to determine
        /// confidence: drift rate is only estimated once sampleCount >= 3,
        /// otherwise defaults to 1/1.
        FilterResult AddSample(Nanos localTime, ProbeResult probe)
        {
            _samples.push_back(Sample{
                .localTime = localTime,
                .offset = probe.offset,
                .rtt = probe.rtt,
            });

            // Evict oldest if over window size.
            if (_samples.size() > _windowSize) {
                _samples.erase(_samples.begin());
            }

            return Compute();
        }

        /// Current number of samples in the window.
        [[nodiscard]] std::size_t SampleCount() const noexcept { return _samples.size(); }

        /// Clear all samples. Used on re-sync.
        void Reset() noexcept { _samples.clear(); }

        /// Access samples (for testing/inspection).
        [[nodiscard]] const std::vector<Sample>& Samples() const noexcept { return _samples; }

    private:
        [[nodiscard]] FilterResult Compute() const
        {
            FilterResult result;
            result.sampleCount = _samples.size();
            result.minRtt = ComputeMinRtt();
            result.offset = ComputeWeightedMedianOffset();
            result.rate   = ComputeDriftRate();
            result.jitter = ComputeJitter();
            return result;
        }

        [[nodiscard]] Nanos ComputeMinRtt() const noexcept
        {
            Nanos minRtt = _samples.front().rtt;
            for (const auto& s : _samples) {
                minRtt = std::min(s.rtt, minRtt);
            }
            return minRtt;
        }

        /// Weighted median of offsets. Weight = 1 / max(rtt, 1).
        /// We use an integer approximation: sort by offset, accumulate
        /// weight until we reach half the total weight.
        [[nodiscard]] Nanos ComputeWeightedMedianOffset() const
        {
            struct WeightedOffset
            {
                Nanos offset;
                std::int64_t weight;
            };

            std::vector<WeightedOffset> sorted;
            sorted.reserve(_samples.size());
            std::int64_t totalWeight = 0;

            for (const auto& s : _samples) {
                // Weight inversely proportional to RTT. Use 1/max(rtt,1) scaled.
                // Scale factor: use the minRtt-based relative weight.
                // Simple: weight = 1'000'000 / max(rtt_ns / 1000, 1) — microsecond-scale RTT.
                Nanos rttUs = std::max(s.rtt / 1000, Nanos{1});
                auto w = static_cast<std::int64_t>(1'000'000 / rttUs);
                w = std::max(w, std::int64_t{1});
                sorted.push_back({s.offset, w});
                totalWeight += w;
            }

            std::ranges::sort(sorted, {}, &WeightedOffset::offset);

            // Walk to the weighted median.
            std::int64_t cumulative = 0;
            std::int64_t halfWeight = totalWeight / 2;
            for (const auto& wo : sorted) {
                cumulative += wo.weight;
                if (cumulative >= halfWeight) {
                    return wo.offset;
                }
            }
            return sorted.back().offset;
        }

        /// Integer linear regression: offset = alpha + beta * (localTime - t0).
        /// Returns Rational rate ≈ 1 + beta (drift per nanosecond).
        ///
        /// Uses: beta = Σ(dx·dy) / Σ(dx²), where dx = localTime - mean(localTime),
        /// dy = offset - mean(offset).
        /// Rate = (den + num_drift) / den where num_drift = beta * den.
        [[nodiscard]] Rational ComputeDriftRate() const
        {
            if (_samples.size() < 3) {
                return Rational{.num=1, .den=1}; // Not enough data for drift.
            }

            const auto n = static_cast<std::int64_t>(_samples.size());

            // Use first sample's localTime as reference to keep values small.
            Nanos t0 = _samples.front().localTime;

            // Scale time and offset from nanoseconds to microseconds before
            // computing sums. The rate ratio (sumXX + sumXY) / sumXX is
            // dimensionless, so dividing both axes by the same constant (1000)
            // preserves it exactly. Without this scaling, (localTime - t0)
            // reaches ~14e9 ns for an 8-sample window at 2s probe intervals,
            // causing dxn² ~ 1.4e21 and sumXX/n ~ 1.7e20 — beyond int64 max
            // (9.2e18) — and the cast below silently wraps to garbage.
            constexpr std::int64_t kScale = 1'000;

            // Compute means (integer division is fine for regression).
            std::int64_t sumDx = 0;
            std::int64_t sumDy = 0;
            for (const auto& s : _samples) {
                sumDx += (s.localTime - t0) / kScale;
                sumDy += s.offset / kScale;
            }
            // meanDx and meanDy in fixed-point (* n to avoid division).
            // dx_centered = (localTime - t0) / kScale * n - sumDx
            // dy_centered = offset / kScale * n - sumDy

            // Compute Σ(dx_centered · dy_centered) and Σ(dx_centered²).
            __int128 sumXY = 0;
            __int128 sumXX = 0;
            for (const auto& s : _samples) {
                auto dxn = static_cast<__int128>((s.localTime - t0) / kScale * n - sumDx);
                auto dyn = static_cast<__int128>(s.offset / kScale * n - sumDy);
                sumXY += dxn * dyn;
                sumXX += dxn * dxn;
            }

            if (sumXX == 0) {
                return Rational{.num=1, .den=1};
            }

            // beta = sumXY / sumXX (drift rate in offset-nanos per local-nanos).
            // We want rate = 1 + beta → rational form.
            // rate = (sumXX + sumXY) / sumXX
            auto num = static_cast<std::int64_t>((sumXX + sumXY) / static_cast<__int128>(n));
            auto den = static_cast<std::int64_t>(sumXX / static_cast<__int128>(n));

            if (den == 0) {
                return Rational{.num=1, .den=1};
            }

            // Simplify by GCD for smaller numbers.
            auto g = Gcd(Abs(num), Abs(den));
            if (g > 1) {
                num /= g;
                den /= g;
            }

            // Ensure denominator is positive.
            if (den < 0) {
                num = -num;
                den = -den;
            }

            return Rational{.num=num, .den=den};
        }

        /// Jitter: interquartile range of offsets.
        [[nodiscard]] Nanos ComputeJitter() const
        {
            if (_samples.size() < 4) {
                // Fallback: range for small windows.
                auto [minIt, maxIt] = std::ranges::minmax_element(
                    _samples, {}, &Sample::offset);
                return maxIt->offset - minIt->offset;
            }

            std::vector<Nanos> offsets;
            offsets.reserve(_samples.size());
            for (const auto& s : _samples) {
                offsets.push_back(s.offset);
            }
            std::ranges::sort(offsets);

            auto q1 = offsets[offsets.size() / 4];
            auto q3 = offsets[offsets.size() * 3 / 4];
            return q3 - q1;
        }

        static constexpr std::int64_t Abs(std::int64_t x) noexcept
        {
            return x < 0 ? -x : x;
        }

        static constexpr std::int64_t Gcd(std::int64_t a, std::int64_t b) noexcept
        {
            while (b != 0) {
                auto t = b;
                b = a % b;
                a = t;
            }
            return a;
        }

        std::size_t _windowSize;
        std::vector<Sample> _samples;
    };
}
