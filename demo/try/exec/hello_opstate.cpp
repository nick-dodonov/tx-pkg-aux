#include "Log/Log.h"

#include <stdexec/execution.hpp>
#include <exec/task.hpp>
#include <exec/static_thread_pool.hpp>

#include <mutex>
#include <condition_variable>
#include <thread>

// ---------------------------------------------------------------------------
// HALO (Heap Allocation ELision Optimization) — visibility rules:
//
// HALO can elide exec::task<T> frame allocation ONLY when:
//   1. The coroutine body is visible at the call site (same TU or inlined header)
//   2. The compiler can prove the frame doesn't outlive the enclosing stack frame
//
// If Read() is in another .cpp — always 1 heap alloc for the frame,
// regardless. LTO may help but is not guaranteed.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// exec::task<void> based coroutine — will always allocate 1 frame.
// There is no way around this if the work is a coroutine.
// ---------------------------------------------------------------------------
static exec::task<void> DoWork(stdexec::inplace_stop_source& stop, std::string path)
{
    Log::Info("[work] started for '{}'", path);

    // Simulate async hop to thread pool
    co_await stdexec::just(); // no-op suspend for illustration

    if (stop.stop_requested())
    {
        Log::Info("[work] cancelled");
        co_return;
    }

    Log::Info("[work] processed '{}'", path);
}

// ---------------------------------------------------------------------------
// ServiceWithSpawn — baseline: scope.spawn() = 2 heap allocs
//   1. exec::task<void> coroutine frame  (from DoWork())
//   2. op-state inside spawn()           (new __spawn_opstate{...})
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Problem with decltype inside a class:
//   A static member function with deduced return type cannot be used in decltype
//   within the same class body — the function body isn't parsed yet at that point.
//
// Fix: move the sender factory OUTSIDE the class as a free function.
//   decltype can then see it fully before the class is defined.
// ---------------------------------------------------------------------------

// Forward-declared outside to allow decltype inside the class body.
// Must be defined before the class if return type is deduced (auto).
static auto MakeInlineOpSender(stdexec::run_loop& loop,
                               stdexec::inplace_stop_source& stop,
                               std::string path)
{
    // starts_on wraps the coroutine call.
    // DoWork(stop, path) allocates 1 coroutine frame on heap — unavoidable.
    // The op-state produced by connect() will be stored inline (see below).
    return stdexec::starts_on(loop.get_scheduler(), DoWork(stop, std::move(path)));
}

// ---------------------------------------------------------------------------
// ServiceInlineOp — stores op-state as direct class member.
//   1. exec::task<void> coroutine frame  (1 heap alloc, unavoidable)
//   2. op-state is a member field        ← NO heap alloc for this
//
// connect_result_t gives the exact op-state type at compile time.
// Stored inline in the object via member-initializer-list + guaranteed
// copy elision (connect() returns prvalue → no move, __immovable is fine).
// ---------------------------------------------------------------------------
class ServiceInlineOp
{
    // --- Receiver -----------------------------------------------------------
    struct Receiver
    {
        using receiver_concept = stdexec::receiver_t;

        ServiceInlineOp* _self;

        void set_value() noexcept       { _self->_notify(); }
        void set_error(auto&&) noexcept { _self->_notify(); }
        void set_stopped() noexcept     { _self->_notify(); }

        auto get_env() const noexcept
        {
            return stdexec::prop{stdexec::get_stop_token, _self->_stop.get_token()};
        }
    };

    void _notify() noexcept
    {
        std::lock_guard lk{_mutex};
        _done = true;
        _cv.notify_one();
    }

    // Free function is fully defined above — decltype works here.
    using _Sender = decltype(MakeInlineOpSender(
        std::declval<stdexec::run_loop&>(),
        std::declval<stdexec::inplace_stop_source&>(),
        std::string{}));

    // connect_result_t: compile-time type of the op-state — stored as member.
    using _Op = stdexec::connect_result_t<_Sender, Receiver>;

    // --- Members: ORDER MATTERS for initializer-list sequencing -------------
    stdexec::run_loop            _loop;                          // 1st: must exist before thread
    std::jthread                 _thread{[this] { _loop.run(); }}; // 2nd: starts, blocks in run()
    stdexec::inplace_stop_source _stop;
    std::mutex                   _mutex;
    std::condition_variable      _cv;
    bool                         _done{false};
    _Op                          _op;  // ← stored inline in the object, no heap

    // --- Non-copyable, non-movable (op-state is __immovable) ----------------
    ServiceInlineOp(const ServiceInlineOp&) = delete;
    ServiceInlineOp& operator=(const ServiceInlineOp&) = delete;

public:
    explicit ServiceInlineOp(std::string path)
        // MakeInlineOpSender() call: allocates 1 coroutine frame on heap
        // connect() call: creates op-state → stored directly in _op (no heap)
        //   guaranteed copy elision: connect() returns prvalue → direct-init of _op
        : _op(stdexec::connect(MakeInlineOpSender(_loop, _stop, std::move(path)), Receiver{this}))
    {
        stdexec::start(_op); // enqueue first step into run_loop (already running)
    }

    // Wait for natural completion (no cancellation).
    void wait()
    {
        std::unique_lock lk{_mutex};
        _cv.wait(lk, [this] { return _done; });
    }

    ~ServiceInlineOp()
    {
        _stop.request_stop();
        std::unique_lock lk{_mutex};
        _cv.wait(lk, [this] { return _done; });
        _loop.finish();
        // _thread joins automatically (std::jthread)
    }
};

// ---------------------------------------------------------------------------
// Zero-alloc variant: no coroutine — plain sender pipeline.
// ONLY possible when logic is expressible without coroutines.
// The entire op-state lives in the object; nothing on heap at all.
// ---------------------------------------------------------------------------

static auto MakeZeroAllocSender(stdexec::run_loop& loop,
                                stdexec::inplace_stop_source& stop,
                                std::string path)
{
    return stdexec::starts_on(
        loop.get_scheduler(),
        stdexec::just(std::move(path))
        | stdexec::then([&stop](std::string p) {
            if (!stop.stop_requested())
                Log::Info("[zero-alloc] processed '{}'", p);
            else
                Log::Info("[zero-alloc] cancelled");
        })
    );
}

class ServiceZeroAlloc
{
    struct Receiver
    {
        using receiver_concept = stdexec::receiver_t;

        ServiceZeroAlloc* _self;

        void set_value() noexcept       { _self->_notify(); }
        void set_error(auto&&) noexcept { _self->_notify(); }
        void set_stopped() noexcept     { _self->_notify(); }

        auto get_env() const noexcept
        {
            return stdexec::prop{stdexec::get_stop_token, _self->_stop.get_token()};
        }
    };

    void _notify() noexcept
    {
        std::lock_guard lk{_mutex};
        _done = true;
        _cv.notify_one();
    }

    static auto _make_sender(stdexec::run_loop& loop,
                             stdexec::inplace_stop_source& stop,
                             std::string path)
    {
        return stdexec::starts_on(
            loop.get_scheduler(),
            stdexec::just(std::move(path))
            | stdexec::then([&stop](std::string p) {
                if (!stop.stop_requested())
                    Log::Info("[zero-alloc] processed '{}'", p);
                else
                    Log::Info("[zero-alloc] cancelled");
            })
        );
    }

    using _Sender = decltype(MakeZeroAllocSender(
        std::declval<stdexec::run_loop&>(),
        std::declval<stdexec::inplace_stop_source&>(),
        std::string{}));
    using _Op = stdexec::connect_result_t<_Sender, Receiver>;

    stdexec::run_loop            _loop;
    std::jthread                 _thread{[this] { _loop.run(); }};
    stdexec::inplace_stop_source _stop;
    std::mutex                   _mutex;
    std::condition_variable      _cv;
    bool                         _done{false};
    _Op                          _op;

    ServiceZeroAlloc(const ServiceZeroAlloc&) = delete;
    ServiceZeroAlloc& operator=(const ServiceZeroAlloc&) = delete;

public:
    explicit ServiceZeroAlloc(std::string path)
        : _op(stdexec::connect(MakeZeroAllocSender(_loop, _stop, std::move(path)), Receiver{this}))
    {
        stdexec::start(_op);
    }

    void wait()
    {
        std::unique_lock lk{_mutex};
        _cv.wait(lk, [this] { return _done; });
    }

    ~ServiceZeroAlloc()
    {
        _stop.request_stop();
        std::unique_lock lk{_mutex};
        _cv.wait(lk, [this] { return _done; });
        _loop.finish();
    }
};

// ---------------------------------------------------------------------------
auto main() -> int
{
    Log::Info("=== ServiceInlineOp: 1 heap alloc (coroutine frame only) ===");
    {
        ServiceInlineOp svc{"config.json"};
        svc.wait(); // wait for natural completion, then dtor cancels (no-op)
    }

    Log::Info("=== ServiceInlineOp: early destruction (cancel) ===");
    {
        ServiceInlineOp svc{"large.dat"};
        // dtor fires immediately — request_stop() + wait for ack
    }

    Log::Info("=== ServiceZeroAlloc: 0 heap allocs (plain sender pipeline) ===");
    {
        ServiceZeroAlloc svc{"config.json"};
        svc.wait();
    }

    Log::Info("=== ServiceZeroAlloc: early destruction (cancel) ===");
    {
        ServiceZeroAlloc svc{"large.dat"};
    }

    return 0;
}
