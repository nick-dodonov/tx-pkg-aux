#include "Log/Log.h"

#include <stdexec/execution.hpp>
#include <exec/task.hpp>
#include <exec/async_scope.hpp>
#include <exec/single_thread_context.hpp>

// Simulated async read — in real code this would be io_uring / asio
// Returns a sender that, when connected+started, produces a string
static auto Read(std::string path)
{
    return stdexec::just(std::string{"content of "} + path);
}

// ---------------------------------------------------------------------------
// FileService: starts reading in constructor, cancels on destruction.
//
// Pattern (analogous to C# Task.Run + CancellationToken):
//
//   Task.Run(() => DoWork(), token)
//       ≈ scope.spawn(stdexec::starts_on(sched, DoWork()))
//         where scope owns the stop token / cancel mechanism
// ---------------------------------------------------------------------------
class FileService
{
public:
    explicit FileService(std::string path)
    {
        Log::Info("[FileService] ctor, starting work");
        // starts_on(sched, task) makes a sender that WILL run on sched.
        // spawn() is what actually CONNECTS + STARTS it — this is the real "Task.Run".
        // No heap allocation for the sender graph; spawn() allocates the op-state once.
        _scope.spawn(stdexec::starts_on(_thread.get_scheduler(), run(std::move(path))));
    }

    ~FileService()
    {
        Log::Info("[FileService] dtor, requesting stop");
        // Signal all tasks inside the scope to stop (injects stop token)
        _scope.request_stop();
        // Wait until all tasks have acknowledged the stop and finished.
        // This sync_wait is acceptable here: destructor *must* be synchronous.
        stdexec::sync_wait(_scope.on_empty());
        Log::Info("[FileService] dtor, all tasks done");
    }

    // Non-copyable, non-movable (owns a thread + live op-states)
    FileService(const FileService&) = delete;
    FileService& operator=(const FileService&) = delete;

private:
    // The coroutine that does the actual work.
    // exec::task automatically propagates the stop token from async_scope
    // through co_await — if stop is requested, inner co_awaits get set_stopped().
    exec::task<void> run(std::string path)
    {
        Log::Info("[run] reading {}", path);
        std::string content = co_await Read(path);

        // Check stop token before doing more work
        auto token = co_await stdexec::get_stop_token();
        if (token.stop_requested())
        {
            Log::Info("[run] stop requested after read, exiting");
            co_return;
        }

        Log::Info("[run] processing: {}", content);
        // ... more co_await steps here, each automatically respects stop token ...
    }

    exec::single_thread_context _thread; // owns one background thread
    exec::async_scope            _scope;  // tracks tasks + owns stop source
};

// ---------------------------------------------------------------------------
// Usage: one sync_wait at the very top — the "main loop" entry point.
// Everything below it is non-blocking async chains with co_await.
// ---------------------------------------------------------------------------
auto main() -> int
{
    Log::Info("=== normal lifecycle ===");
    {
        FileService svc{"config.json"};
        // Service is running async work in background.
        // Here we could do other work, handle events, etc.
        // Destructor will wait for clean shutdown.
    }

    Log::Info("=== early destruction (cancel) ===");
    {
        FileService svc{"large_file.dat"};
        // Immediately destroyed — destructor cancels the task
    }

    return 0;
}
