#include "Boot/Boot.h"
#include "Log/Log.h"

#include <boost/capy.hpp>

namespace capy = boost::capy;

// Simplest possible executor - executes coroutines immediately
// No queue, no thread switching, just inline execution
class inline_executor_context : public capy::execution_context
{
public:
    class executor_type;

    inline_executor_context()
        : execution_context(this)
    {
    }

    ~inline_executor_context()
    {
        shutdown();
        destroy();
    }

    executor_type get_executor() noexcept;
};

class inline_executor_context::executor_type
{
    friend class inline_executor_context;
    inline_executor_context* context_ = nullptr;

    explicit executor_type(inline_executor_context& ctx) noexcept
        : context_(&ctx)
    {
    }

public:
    executor_type() = default;

    capy::execution_context& context() const noexcept
    {
        return *context_;
    }

    void on_work_started() const noexcept {}
    void on_work_finished() const noexcept {}

    // Always execute immediately - no queuing
    std::coroutine_handle<> dispatch(std::coroutine_handle<> h) const
    {
        return h; // Execute immediately
    }

    // Post is the same as dispatch for inline execution
    void post(std::coroutine_handle<> h) const
    {
        h.resume(); // Execute immediately
    }

    bool operator==(executor_type const& other) const noexcept
    {
        return context_ == other.context_;
    }
};

inline inline_executor_context::executor_type 
inline_executor_context::get_executor() noexcept
{
    return executor_type{*this};
}

static_assert(capy::Executor<inline_executor_context::executor_type>);

// Test coroutines
capy::task<int> get_value(int x)
{
    Log::Info("get_value({}) on thread {}", x, std::this_thread::get_id());
    co_return x * 2;
}

capy::task<void> process()
{
    Log::Info("process() start on thread {}", std::this_thread::get_id());
    
    auto v1 = co_await get_value(10);
    Log::Info("Got v1: {}", v1);
    
    auto v2 = co_await get_value(20);
    Log::Info("Got v2: {}", v2);
    
    Log::Info("process() done, total: {}", v1 + v2);
}

int main(const int argc, const char** argv)
{
    Boot::DefaultInit(argc, argv);

    Log::Info("=== Inline Executor Example ===");
    Log::Info("Main thread ID: {}", std::this_thread::get_id());
    
    inline_executor_context ctx;
    
    Log::Info("\nLaunching coroutine...");
    capy::run_async(ctx.get_executor())(process());
    
    Log::Info("\nAll coroutines completed synchronously");
    Log::Info("No event loop needed - everything executed inline");
    
    return 0;
}
