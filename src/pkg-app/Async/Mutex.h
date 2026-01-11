#pragma once
#include "Async/ThreadSafety.h"

#include <mutex>

namespace Async
{
    /// Mutex wrapper with thread safety annotations
    class ASYNC_CAPABILITY("mutex") Mutex
    {
    public:
        void lock() ASYNC_ACQUIRE() { _impl.lock(); }
        void unlock() ASYNC_RELEASE() { _impl.unlock(); }
        bool try_lock() { return _impl.try_lock(); }

    private:
        std::mutex _impl;
    };

    /// Lock guard wrapper with thread safety annotations
    class ASYNC_SCOPED_CAPABILITY LockGuard
    {
    public:
        explicit LockGuard(Mutex& mutex) ASYNC_ACQUIRE(mutex)
            : _mutex(mutex)
        {
            _mutex.lock();
        }

        ~LockGuard() ASYNC_RELEASE()
        {
            _mutex.unlock();
        }

        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;
        LockGuard(LockGuard&&) = delete;
        LockGuard& operator=(LockGuard&&) = delete;

    private:
        Mutex& _mutex;
    };
}

