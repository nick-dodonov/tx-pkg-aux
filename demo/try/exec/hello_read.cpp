#include "Log/Log.h"

#include <stdexec/execution.hpp>
#include <exec/task.hpp>
#include <exec/async_scope.hpp>
#include <exec/static_thread_pool.hpp>

#include <unordered_map>
#include <mutex>
#include <string>

// ---------------------------------------------------------------------------
// Global I/O thread pool: 2 worker threads.
// Read() schedules heavy work here, caller knows nothing about this.
// ---------------------------------------------------------------------------
static exec::static_thread_pool io_pool{2};

// ---------------------------------------------------------------------------
// Simple in-memory cache (thread-safe)
// ---------------------------------------------------------------------------
struct Cache
{
    std::optional<std::string> get(const std::string& key)
    {
        std::lock_guard lk{_mutex};
        if (auto it = _map.find(key); it != _map.end())
            return it->second;
        return std::nullopt;
    }

    void put(const std::string& key, const std::string& value)
    {
        std::lock_guard lk{_mutex};
        _map[key] = value;
    }

private:
    std::mutex _mutex;
    std::unordered_map<std::string, std::string> _map;
};

static Cache cache;

// ---------------------------------------------------------------------------
// Read(path) — the public API.
//
// Caller doesn't know (and doesn't care):
//   - Is it cached?         → returns immediately, no thread hop
//   - Is it a real read?    → goes to io_pool, comes back with result
//
// Usage from a coroutine:
//   std::string content = co_await Read("file.txt");
// ---------------------------------------------------------------------------
static exec::task<std::string> Read(std::string path)
{
    // Fast path: cache hit — no scheduling, returns in the caller's context
    if (auto cached = cache.get(path))
    {
        Log::Info("[Read] cache hit for '{}'", path);
        co_return *cached;
    }

    // Slow path: schedule onto io_pool, do the "read", come back
    // starts_on transfers execution to io_pool for this co_await
    co_await stdexec::starts_on(io_pool.get_scheduler(), stdexec::just());

    Log::Info("[Read] reading '{}' on thread pool", path);
    // Simulate disk read (would be real I/O here)
    std::string content = "content of <" + path + ">";

    cache.put(path, content);
    co_return content;
}

// ---------------------------------------------------------------------------
// FileService — owns its lifecycle; caller just constructs and destroys.
// The entire async logic is one coroutine: no visible receivers, no op-states.
// ---------------------------------------------------------------------------
// FileService — owns its lifecycle; caller just constructs and destroys.
// The entire async logic is one coroutine: no visible receivers, no op-states.
// ---------------------------------------------------------------------------
class FileService
{
public:
    explicit FileService(exec::static_thread_pool& work_pool, std::string path)
    {
        Log::Info("[FileService] starting");
        _scope.spawn(stdexec::starts_on(work_pool.get_scheduler(), run(std::move(path))));
    }

    // Sender that completes when all work finishes naturally.
    // Caller decides when and how to wait — no forced sync here.
    auto on_complete() { return _scope.on_empty(); }

    // Explicit cancel: stop all in-flight work, return sender to await cleanup.
    auto cancel()
    {
        _scope.request_stop();
        return _scope.on_empty();
    }

    ~FileService()
    {
        // If caller didn't explicitly cancel, wait for natural completion.
        // If cancel() was called, on_empty() returns immediately (already done).
        _scope.request_stop();
        stdexec::sync_wait(_scope.on_empty());
        Log::Info("[FileService] shutdown complete");
    }

private:
    exec::task<void> run(std::string path)
    {
        // Read decides itself where to run — we just await the result
        std::string content = co_await Read(path);

        auto token = co_await stdexec::get_stop_token();
        if (token.stop_requested())
        {
            Log::Info("[FileService] cancelled after read");
            co_return;
        }

        Log::Info("[FileService] processing: {}", content);

        // Second read — cache hit, returns immediately without thread hop
        std::string again = co_await Read(path);
        Log::Info("[FileService] second read (cached): {}", again);
    }

    exec::async_scope _scope;
};

// ---------------------------------------------------------------------------
auto main() -> int
{
    exec::static_thread_pool work_pool{4};

    Log::Info("=== normal lifecycle: wait for natural completion ===");
    {
        FileService svc{work_pool, "config.json"};
        // Caller awaits completion — no cancellation
        stdexec::sync_wait(svc.on_complete());
        // dtor: request_stop (no-op, already done) + on_empty (instant)
    }

    Log::Info("=== second instance: first read is now cached ===");
    {
        FileService svc{work_pool, "config.json"};
        stdexec::sync_wait(svc.on_complete());
    }

    Log::Info("=== early destruction: explicit cancel then dtor ===");
    {
        FileService svc{work_pool, "large_file.dat"};
        // Immediately cancel without waiting
        // dtor will request_stop + wait for the cancellation ack
    }

    return 0;
}
