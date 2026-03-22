#pragma once
#include "IDelayBackend.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_set>
#include <vector>

namespace Exec
{
    /// Thread-based timer backend. The default choice on desktop platforms.
    ///
    /// Runs a dedicated background thread that sleeps until the next deadline,
    /// then fires the registered callback. The callback is invoked from that
    /// background thread, so it must be safe to call from a non-main thread
    /// (the CAS in DelaySharedState guarantees this is safe: it only enqueues
    /// an OperationBase pointer into the lock-free ConcurrentQueue).
    ///
    /// Cancel() removes the entry from the active set before invocation. If the
    /// timer thread is already executing the callback, Cancel() returns false
    /// immediately — it does NOT block. Callers must tolerate a late-fire.
    class ThreadDelayBackend : public IDelayBackend
    {
    public:
        ThreadDelayBackend();
        ~ThreadDelayBackend() override;

        ThreadDelayBackend(const ThreadDelayBackend&) = delete;
        ThreadDelayBackend& operator=(const ThreadDelayBackend&) = delete;

        TimerId ScheduleAt(TimePoint deadline, Callback callback) override;
        bool Cancel(TimerId id) noexcept override;
        // Tick() intentionally left as no-op: callbacks fire from the timer thread.

    private:
        struct Entry
        {
            TimePoint deadline;
            TimerId   id;
            Callback  callback;

            // Min-heap ordering: smallest deadline at the top.
            bool operator>(const Entry& rhs) const noexcept { return deadline > rhs.deadline; }
        };

        void RunTimerThread();

        std::mutex _mutex;
        std::condition_variable _cv;
        std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> _queue;
        std::unordered_set<TimerId> _active; // ids currently waiting (not yet fired / cancelled)
        std::atomic<bool> _stopping{false};
        std::atomic<TimerId> _nextId{1};
        std::jthread _thread;
    };

} // namespace Exec
