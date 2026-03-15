#include "Boot/Boot.h"
#include "Log/Log.h"

#include <boost/capy.hpp>

#include <queue>
#include <thread>

namespace capy = boost::capy;

// Single-threaded execution context for running coroutines on the main thread
// Similar to a GUI event loop - no thread pool involved
class run_loop : public capy::execution_context
{
    std::queue<std::coroutine_handle<>> queue_;
    std::thread::id owner_;

public:
    class executor_type;

    run_loop()
        : execution_context(this)
    {
    }

    ~run_loop()
    {
        shutdown();
        destroy();
    }

    run_loop(run_loop const&) = delete;
    run_loop& operator=(run_loop const&) = delete;

    // Drain the queue until empty - all coroutines execute on this thread
    void run()
    {
        owner_ = std::this_thread::get_id();
        Log::Info("Starting event loop on thread {}", owner_);
        
        while (!queue_.empty())
        {
            auto h = queue_.front();
            queue_.pop();
            h.resume();
        }
        
        Log::Info("Event loop finished");
    }

    void enqueue(std::coroutine_handle<> h)
    {
        queue_.push(h);
    }

    bool is_running_on_this_thread() const noexcept
    {
        return std::this_thread::get_id() == owner_;
    }

    executor_type get_executor() noexcept;
};

class run_loop::executor_type
{
    friend class run_loop;
    run_loop* loop_ = nullptr;

    explicit executor_type(run_loop& loop) noexcept
        : loop_(&loop)
    {
    }

public:
    executor_type() = default;

    capy::execution_context& context() const noexcept
    {
        return *loop_;
    }

    void on_work_started() const noexcept {}
    void on_work_finished() const noexcept {}

    // If already on the correct thread, execute immediately
    // Otherwise enqueue for later execution
    std::coroutine_handle<> dispatch(std::coroutine_handle<> h) const
    {
        if (loop_->is_running_on_this_thread()) {
            return h;
        }
        loop_->enqueue(h);
        return std::noop_coroutine();
    }

    void post(std::coroutine_handle<> h) const
    {
        loop_->enqueue(h);
    }

    bool operator==(executor_type const& other) const noexcept
    {
        return loop_ == other.loop_;
    }
};

inline run_loop::executor_type run_loop::get_executor() noexcept
{
    return executor_type{*this};
}

// Verify the Executor concept is satisfied
static_assert(capy::Executor<run_loop::executor_type>);

// A coroutine that returns a value and logs the thread ID
capy::task<int> answer()
{
    Log::Info("answer() executing on thread {}", std::this_thread::get_id());
    co_return 42;
}

// A coroutine that awaits multiple other coroutines
capy::task<void> compute_values()
{
    Log::Info("compute_values() start on thread {}", std::this_thread::get_id());
    
    auto result1 = co_await answer();
    Log::Info("Got result1: {}", result1);
    
    auto result2 = co_await answer();
    Log::Info("Got result2: {}", result2);
    
    Log::Info("compute_values() done, sum: {}", result1 + result2);
}

// A more complex example with when_all
capy::task<int> compute_square(int x)
{
    Log::Info("Computing {}^2 on thread {}", x, std::this_thread::get_id());
    co_return x * x;
}

capy::task<void> parallel_computation()
{
    Log::Info("Launching parallel tasks with when_all on thread {}", 
              std::this_thread::get_id());

    auto [a, b, c] = co_await capy::when_all(
        compute_square(3),
        compute_square(7),
        compute_square(11));

    Log::Info("Results: {}, {}, {} (sum: {})", a, b, c, a + b + c);
}

int main(const int argc, const char** argv)
{
    Boot::DefaultInit(argc, argv);

    Log::Info("=== Main thread execution example ===");
    Log::Info("Main thread ID: {}", std::this_thread::get_id());
    
    // Create a run_loop instead of thread_pool
    run_loop loop;

    // Launch coroutines using run_async with the loop's executor
    Log::Info("\n--- Example 1: Sequential coroutines ---");
    capy::run_async(loop.get_executor())(compute_values());
    loop.run();

    // Create a new loop for the second example
    run_loop loop2;
    Log::Info("\n--- Example 2: Parallel coroutines (still on main thread) ---");
    capy::run_async(loop2.get_executor())(parallel_computation());
    loop2.run();

    Log::Info("\nAll coroutines executed on the main thread");
    return 0;
}
