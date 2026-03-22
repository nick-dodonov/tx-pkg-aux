#include "Exec/Context/TimedLoopContext.h"
#include "Exec/Delay/LoopTimerBackend.h"

#include <gtest/gtest.h>
#include <stdexec/execution.hpp>

#include <algorithm>
#include <chrono>
#include <utility>
#include <vector>

namespace {

using namespace Exec;
using namespace std::chrono_literals;
using TimePoint = ITimerBackend::TimePoint;
using TimerId   = ITimerBackend::TimerId;

// ---------------------------------------------------------------------------
// FakeTimerBackend — deterministic, tick-controlled timer backend for tests
// ---------------------------------------------------------------------------

struct FakeTimerBackend : ITimerBackend
{
    struct Entry
    {
        TimePoint         deadline;
        TimerId           id;
        ITimerBackend::Callback callback;
    };

    std::vector<Entry>   scheduled;
    std::vector<TimerId> cancelled;
    int                  tickCount = 0;
    TimerId              nextId{1};

    TimerId ScheduleAt(TimePoint dl, Callback cb) override
    {
        const TimerId id = nextId++;
        scheduled.push_back({dl, id, std::move(cb)});
        return id;
    }

    bool Cancel(TimerId id) noexcept override
    {
        cancelled.push_back(id);
        auto it = std::ranges::find_if(scheduled, [id](const Entry& e) { return e.id == id; });
        if (it == scheduled.end()) {
            return false;
        }
        scheduled.erase(it);
        return true;
    }

    void Tick() noexcept override { ++tickCount; }

    /// Fire all pending timer callbacks (simulates every timer expiring).
    void FireAll()
    {
        auto snap = std::exchange(scheduled, {});
        for (auto& e : snap) {
            e.callback();
        }
    }

    /// Fire only callbacks whose deadline is at or before `now`.
    void FireExpired(TimePoint now)
    {
        std::vector<Entry> remaining;
        for (auto& e : scheduled)
        {
            if (e.deadline <= now) {
                e.callback();
            } else {
                remaining.push_back(std::move(e));
            }
        }
        scheduled = std::move(remaining);
    }

    [[nodiscard]] bool WasCancelled(TimerId id) const noexcept
    {
        return std::ranges::find(cancelled, id) != cancelled.end();
    }
};

// ---------------------------------------------------------------------------
// Time helpers
// ---------------------------------------------------------------------------

[[nodiscard]] inline auto Past()   { return std::chrono::steady_clock::now() - 1h; }
[[nodiscard]] inline auto Future() { return std::chrono::steady_clock::now() + 1h; }

// ---------------------------------------------------------------------------
// Shared receiver types (same pattern as pure_loop_context_test.cpp)
// ---------------------------------------------------------------------------

struct Outcome
{
    bool valued  = false;
    bool stopped = false;
};

/// Receiver with no stop token — never-stop fallback.
struct BasicReceiver
{
    using receiver_concept = stdexec::receiver_t;

    Outcome* outcome;

    [[nodiscard]] auto get_env() const noexcept { return stdexec::env<>{}; }
    void set_value() const noexcept  { outcome->valued  = true; }
    void set_stopped() const noexcept { outcome->stopped = true; }
    void set_error(auto&&) noexcept {}
};

/// Environment that provides a concrete inplace_stop_token.
struct StopEnv
{
    stdexec::inplace_stop_token token;

    [[nodiscard]] auto query(stdexec::get_stop_token_t) const noexcept { return token; }
};

/// Receiver that exposes a stop token — needed for set_stopped tests.
struct StopReceiver
{
    using receiver_concept = stdexec::receiver_t;

    Outcome*                    outcome;
    stdexec::inplace_stop_token token;

    [[nodiscard]] auto get_env() const noexcept { return StopEnv{token}; }
    void set_value() const noexcept  { outcome->valued  = true; }
    void set_stopped() const noexcept { outcome->stopped = true; }
    void set_error(auto&&) noexcept {}
};

} // namespace

// ---------------------------------------------------------------------------
// LoopSender (TimedLoopContext::Scheduler::schedule())
// ---------------------------------------------------------------------------

// schedule() must queue work at the next drain boundary, not on start().
TEST(LoopSenderTest, NoExecutionBeforeDrain)
{
    FakeTimerBackend fake;
    TimedLoopContext ctx{&fake};
    Outcome outcome;

    auto op = stdexec::connect(ctx.GetScheduler().schedule(), BasicReceiver{&outcome});
    stdexec::start(op);

    EXPECT_FALSE(outcome.valued);
    EXPECT_FALSE(outcome.stopped);
}

TEST(LoopSenderTest, DrainDeliversSetValue)
{
    FakeTimerBackend fake;
    TimedLoopContext ctx{&fake};
    Outcome outcome;

    auto op = stdexec::connect(ctx.GetScheduler().schedule(), BasicReceiver{&outcome});
    stdexec::start(op);
    ctx.DrainQueue();

    EXPECT_TRUE(outcome.valued);
    EXPECT_FALSE(outcome.stopped);
}

// Stop requested between start() and DrainQueue() → set_stopped at drain time.
TEST(LoopSenderTest, StopAfterStart_DeliversSetStopped)
{
    FakeTimerBackend fake;
    TimedLoopContext ctx{&fake};
    Outcome outcome;
    stdexec::inplace_stop_source source;

    auto op = stdexec::connect(ctx.GetScheduler().schedule(),
                               StopReceiver{.outcome = &outcome, .token = source.get_token()});
    stdexec::start(op);
    source.request_stop();
    ctx.DrainQueue();

    EXPECT_FALSE(outcome.valued);
    EXPECT_TRUE(outcome.stopped);
}

// get_completion_scheduler(get_env(schedule(sched))) must equal sched.
TEST(LoopSenderTest, CompletionScheduler_MatchesOriginalScheduler)
{
    FakeTimerBackend fake;
    TimedLoopContext ctx{&fake};
    auto sched = ctx.GetScheduler();

    auto sender = sched.schedule();
    auto env    = stdexec::get_env(sender);
    auto cs     = stdexec::get_completion_scheduler<stdexec::set_value_t>(env);

    EXPECT_EQ(cs, sched);
}

// ---------------------------------------------------------------------------
// TimedSender (schedule_at / schedule_after)
// ---------------------------------------------------------------------------

// Timer fires before DrainQueue → state enqueued → set_value on drain.
TEST(TimedSenderTest, TimerFiresBeforeDrain_DeliversSetValue)
{
    FakeTimerBackend fake;
    TimedLoopContext ctx{&fake};
    Outcome outcome;

    auto op = stdexec::connect(ctx.GetScheduler().schedule_at(Future()),
                               BasicReceiver{&outcome});
    stdexec::start(op);

    ASSERT_EQ(fake.scheduled.size(), 1u); // timer registered
    fake.FireAll();                        // timer fires → enqueues state

    EXPECT_FALSE(outcome.valued);  // not yet — pending drain
    ctx.DrainQueue();
    EXPECT_TRUE(outcome.valued);
    EXPECT_FALSE(outcome.stopped);
}

// Timer not yet fired, DrainQueue runs → nothing happens.
TEST(TimedSenderTest, FutureTimer_NoResultBeforeFire)
{
    FakeTimerBackend fake;
    TimedLoopContext ctx{&fake};
    Outcome outcome;

    auto op = stdexec::connect(ctx.GetScheduler().schedule_at(Future()),
                               BasicReceiver{&outcome});
    stdexec::start(op);

    ctx.DrainQueue(); // Tick() called but timer not expired — nothing queued

    EXPECT_FALSE(outcome.valued);
    EXPECT_FALSE(outcome.stopped);
    // keep op alive so destructor doesn't cancel mid-test unexpectedly
    (void)fake.scheduled.size(); // suppress "op unused" warning
}

// Stop requested after start() but before timer fires → stop wins CAS → set_stopped.
TEST(TimedSenderTest, StopBeforeTimer_DeliversSetStopped)
{
    FakeTimerBackend fake;
    TimedLoopContext ctx{&fake};
    Outcome outcome;
    stdexec::inplace_stop_source source;

    auto op = stdexec::connect(ctx.GetScheduler().schedule_at(Future()),
                               StopReceiver{.outcome = &outcome, .token = source.get_token()});
    stdexec::start(op);

    source.request_stop(); // stop callback fires synchronously, wins CAS, enqueues state
    ctx.DrainQueue();      // executes set_stopped

    EXPECT_FALSE(outcome.valued);
    EXPECT_TRUE(outcome.stopped);
}

// Stop pre-requested before start() → stop_callback_for_t fires StopCallback
// synchronously during stopCb.emplace(), winning the CAS. start() skips
// ScheduleAt and returns early — no timer is registered.
TEST(TimedSenderTest, PreStop_DeliversSetStopped)
{
    FakeTimerBackend fake;
    TimedLoopContext ctx{&fake};
    Outcome outcome;
    stdexec::inplace_stop_source source;
    source.request_stop(); // stop before start

    auto op = stdexec::connect(ctx.GetScheduler().schedule_at(Future()),
                               StopReceiver{.outcome = &outcome, .token = source.get_token()});
    stdexec::start(op); // stop callback fires synchronously; ScheduleAt skipped

    EXPECT_EQ(fake.scheduled.size(), 0u); // no timer registered
    ctx.DrainQueue();
    EXPECT_FALSE(outcome.valued);
    EXPECT_TRUE(outcome.stopped);
}

// Timer fires first, winning the CAS (State::TimerWon). Stop is then requested
// before DrainQueue. Execute checks stop_requested() at drain time (frame-accurate
// cancellation) → set_stopped, even though the timer won the CAS race.
TEST(TimedSenderTest, TimerWinsCAS_StopBeforeDrain_DeliversSetStopped)
{
    FakeTimerBackend fake;
    TimedLoopContext ctx{&fake};
    Outcome outcome;
    stdexec::inplace_stop_source source;

    auto op = stdexec::connect(ctx.GetScheduler().schedule_at(Future()),
                               StopReceiver{.outcome = &outcome, .token = source.get_token()});
    stdexec::start(op);

    fake.FireAll();        // timer wins CAS → State::TimerWon, enqueued
    source.request_stop(); // stop callback loses CAS, but stop_requested() is now true
    ctx.DrainQueue();      // Execute: stop_requested() → set_stopped

    EXPECT_FALSE(outcome.valued);
    EXPECT_TRUE(outcome.stopped);
}

// schedule_after delegates to schedule_at(now + dur) — verify set_value after fire.
TEST(TimedSenderTest, ScheduleAfter_FiresAfterDuration)
{
    FakeTimerBackend fake;
    TimedLoopContext ctx{&fake};
    Outcome outcome;

    auto op = stdexec::connect(ctx.GetScheduler().schedule_after(500ms),
                               BasicReceiver{&outcome});
    stdexec::start(op);

    ASSERT_EQ(fake.scheduled.size(), 1u);
    fake.FireAll();
    ctx.DrainQueue();

    EXPECT_TRUE(outcome.valued);
}

// ---------------------------------------------------------------------------
// Destructor / Cancel hygiene
// ---------------------------------------------------------------------------

// When stop wins CAS and set_stopped is delivered, TryEnqueueStop() eagerly cancels
// the backend timer — removing the pending entry immediately, not waiting for the destructor.
TEST(DestructorTest, StopWins_TimerCancelledEagerly)
{
    FakeTimerBackend fake;
    TimedLoopContext ctx{&fake};
    Outcome outcome;
    stdexec::inplace_stop_source source;

    {
        auto op = stdexec::connect(ctx.GetScheduler().schedule_at(Future()),
                                   StopReceiver{.outcome = &outcome, .token = source.get_token()});
        stdexec::start(op);

        ASSERT_EQ(fake.scheduled.size(), 1u);

        source.request_stop(); // stop wins CAS → TryEnqueueStop cancels timer eagerly
        ctx.DrainQueue();      // set_stopped delivered
        ASSERT_TRUE(outcome.stopped);

        // op destructor: timerId already cleared by TryEnqueueStop → no second Cancel
    }

    EXPECT_TRUE(fake.WasCancelled(1)); // FakeTimerBackend assigns id 1 to first timer
}

// When the timer wins and set_value is delivered, the timer callback has already cleared
// timerId — so the destructor skips Cancel. No redundant cancel for a timer that already fired.
TEST(DestructorTest, TimerWins_NoCancelOnOpDestruction)
{
    FakeTimerBackend fake;
    TimedLoopContext ctx{&fake};
    Outcome outcome;

    {
        auto op = stdexec::connect(ctx.GetScheduler().schedule_at(Future()),
                                   BasicReceiver{&outcome});
        stdexec::start(op);

        fake.FireAll();    // timer fires, clears timerId, wins CAS, enqueues
        ctx.DrainQueue();  // set_value delivered
        ASSERT_TRUE(outcome.valued);

        // op destructor: timerId == InvalidTimerId (cleared in lambda) → Cancel not called
    }

    EXPECT_FALSE(fake.WasCancelled(1)); // timer already fired — no Cancel needed or issued
}

// ---------------------------------------------------------------------------
// DrainQueue wiring
// ---------------------------------------------------------------------------

// DrainQueue() must call Tick() on the backend once per invocation so that
// loop-integrated timer backends (LoopTimerBackend) can fire expired entries.
TEST(TickTest, DrainQueue_CallsBackendTickExactlyOnce)
{
    FakeTimerBackend fake;
    TimedLoopContext ctx{&fake};

    EXPECT_EQ(fake.tickCount, 0);
    ctx.DrainQueue();
    EXPECT_EQ(fake.tickCount, 1);
    ctx.DrainQueue();
    EXPECT_EQ(fake.tickCount, 2);
}

// Tick() is called before draining the queue, so timers fired in Tick() are
// processed in the same DrainQueue() call — tested via FireExpired integration.
TEST(TickTest, DrainQueue_ProcessesTimersAndLoopWorkInSameCall)
{
    // Use LoopTimerBackend for this integration test — it fires timers in Tick().
    LoopTimerBackend loopBackend;
    TimedLoopContext ctx{&loopBackend};
    Outcome outcome;

    auto op = stdexec::connect(ctx.GetScheduler().schedule_at(Past()),
                               BasicReceiver{&outcome});
    stdexec::start(op);

    // Single DrainQueue call: Tick() fires the past-deadline timer (enqueues state),
    // then DrainQueue processes the state → set_value.
    ctx.DrainQueue();

    EXPECT_TRUE(outcome.valued);
    EXPECT_FALSE(outcome.stopped);
}
