#include "ThreadTimerBackend.h"

namespace Exec
{
    ThreadTimerBackend::ThreadTimerBackend()
        : _thread([this] { RunTimerThread(); })
    {}

    ThreadTimerBackend::~ThreadTimerBackend()
    {
        _stopping.store(true, std::memory_order_relaxed);
        _cv.notify_all();
        // jthread destructor joins automatically.
    }

    auto ThreadTimerBackend::ScheduleAt(TimePoint deadline, Callback callback) -> TimerId
    {
        const TimerId id = _nextId.fetch_add(1, std::memory_order_relaxed);
        {
            std::lock_guard lock{_mutex};
            _active.insert(id);
            _queue.push({deadline, id, std::move(callback)});
        }
        _cv.notify_one();
        return id;
    }

    bool ThreadTimerBackend::Cancel(TimerId id) noexcept
    {
        std::unique_lock lock{_mutex};
        // Block until any in-flight execution of this callback has finished.
        // Invariant: once the callback exits, RunTimerThread clears _inflightId
        // and notifies _doneCv — so this wait always terminates.
        _doneCv.wait(lock, [this, id] { return _inflightId != id; });
        return _active.erase(id) > 0;
    }

    void ThreadTimerBackend::RunTimerThread()
    {
        std::unique_lock lock{_mutex};
        while (!_stopping.load(std::memory_order_relaxed)) {
            if (_queue.empty()) {
                _cv.wait(lock, [this] {
                    return !_queue.empty() || _stopping.load(std::memory_order_relaxed);
                });
                continue;
            }

            const auto& top = _queue.top();
            const auto deadline = top.deadline;

            // Wait until the top deadline or until something changes (new entry, cancel, stop).
            if (_cv.wait_until(lock, deadline) == std::cv_status::timeout
                || std::chrono::steady_clock::now() >= deadline)
            {
                // Pop entries that are due and still active.
                while (!_queue.empty() && std::chrono::steady_clock::now() >= _queue.top().deadline) {
                    auto entry = _queue.top(); // copy
                    _queue.pop();

                    if (_active.erase(entry.id) == 0) {
                        // Already cancelled — skip.
                        continue;
                    }

                    // Mark as in-flight so Cancel() can block until we finish.
                    _inflightId = entry.id;

                    // Invoke callback with the lock released so the callback can
                    // safely call back into ScheduleAt/Cancel without deadlocking.
                    lock.unlock();
                    entry.callback();
                    lock.lock();

                    _inflightId = InvalidTimerId;
                    _doneCv.notify_all();
                }
            }
        }
    }
}
