#pragma once
#include "IDelayBackend.h"
#include <algorithm>
#include <atomic>
#include <vector>

namespace Exec
{
    /// Frame-loop-integrated timer backend. No background thread.
    ///
    /// Callbacks fire synchronously when Tick() is called from the main/update
    /// thread (via Domain::Update → TimedScheduler::TickBackend → Tick()), which
    /// makes behaviour deterministic and easy to test: advance time by calling
    /// Tick() repeatedly.
    ///
    /// Thread safety: all methods are intended to be called from the same thread.
    /// ScheduleAt() and Cancel() from start() / stop-callback (both run in the
    /// main/update thread context via the domain lock-free queue), and Tick() from
    /// Domain::Update(). If you need cross-thread use, use ThreadDelayBackend instead.
    class LoopDelayBackend : public IDelayBackend
    {
    public:
        TimerId ScheduleAt(TimePoint deadline, Callback callback) override
        {
            const TimerId id = _nextId.fetch_add(1, std::memory_order_relaxed);
            auto it = std::lower_bound(_entries.begin(), _entries.end(), deadline,
                [](const Entry& e, const TimePoint& tp) { return e.deadline < tp; });
            _entries.insert(it, {deadline, id, std::move(callback)});
            return id;
        }

        bool Cancel(TimerId id) noexcept override
        {
            auto it = std::find_if(_entries.begin(), _entries.end(),
                [id](const Entry& e) { return e.id == id; });
            if (it == _entries.end()) {
                return false;
            }
            _entries.erase(it);
            return true;
        }

        /// Fire all callbacks whose deadline is at or before steady_clock::now().
        ///
        /// Called from the main thread (Domain::Update → TimedScheduler::TickBackend).
        /// Callbacks are invoked in deadline order. Entries added during a callback
        /// are NOT fired in the same Tick() call (they appear after the current
        /// snapshot boundary).
        void Tick() noexcept override
        {
            const auto now = std::chrono::steady_clock::now();
            // Fire front entries while they are due — entries are sorted by deadline.
            while (!_entries.empty() && _entries.front().deadline <= now) {
                // Move the entry out before invoking callback so a callback that
                // calls ScheduleAt() does not corrupt the iteration.
                auto entry = std::move(_entries.front());
                _entries.erase(_entries.begin());
                entry.callback();
            }
        }

    private:
        struct Entry
        {
            TimePoint deadline;
            TimerId   id;
            Callback  callback;
        };

        // Sorted ascending by deadline (front = soonest).
        std::vector<Entry> _entries;
        std::atomic<TimerId> _nextId{1};
    };

} // namespace Exec
