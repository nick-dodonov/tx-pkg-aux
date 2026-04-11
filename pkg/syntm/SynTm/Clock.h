#pragma once
#include "Types.h"

#include <chrono>

namespace SynTm
{
    /// Abstract clock source for testability.
    /// Production code uses SteadyClock; tests use FakeClock.
    class IClock
    {
    public:
        virtual ~IClock() = default;

        /// Current time in ticks since an arbitrary epoch.
        [[nodiscard]] virtual Ticks Now() const noexcept = 0;
    };

    /// Production clock wrapping std::chrono::steady_clock.
    class SteadyClock final : public IClock
    {
    public:
        [[nodiscard]] Ticks Now() const noexcept override
        {
            auto tp = std::chrono::steady_clock::now();
            return std::chrono::duration_cast<std::chrono::nanoseconds>(
                       tp.time_since_epoch())
                .count();
        }
    };

    /// Clock that returns ticks elapsed since construction.
    /// Produces human-friendly timestamps starting near zero.
    class AppClock final : public IClock
    {
    public:
        [[nodiscard]] Ticks Now() const noexcept override
        {
            auto elapsed = std::chrono::steady_clock::now() - _epoch;
            return std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
        }

    private:
        std::chrono::steady_clock::time_point _epoch = std::chrono::steady_clock::now();
    };

    /// Deterministic clock for testing. Time only advances when explicitly told.
    class FakeClock final : public IClock
    {
    public:
        [[nodiscard]] Ticks Now() const noexcept override { return _now; }

        void SetNow(Ticks value) noexcept { _now = value; }

        void Advance(Ticks delta) noexcept { _now += delta; }

    private:
        Ticks _now = 0;
    };
}
