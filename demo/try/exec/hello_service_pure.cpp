#include "Log/Log.h"

#include <stdexec/execution.hpp>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <optional>

// Simulated async read sender
static auto Read(std::string path)
{
    return stdexec::just(std::string{"content of "} + path);
}

// ---------------------------------------------------------------------------
// Approach 1: std::jthread + sync_wait on background thread
//
// Pro:  Simplest possible pure stdexec. No custom receivers, no op-state storage.
// Con:  Background thread blocks inside sync_wait until work completes.
//       One thread per chain. Use when chains are short-lived or infrequent.
// ---------------------------------------------------------------------------
class FileServiceSimple
{
public:
    explicit FileServiceSimple(std::string path)
    {
        Log::Info("[Simple] ctor, spawning work");
        _thread = std::jthread([path = std::move(path)](std::stop_token token) {
            // sync_wait here blocks the background thread, NOT main thread.
            // This is the cost of pure stdexec without exec::task.
            stdexec::sync_wait(
                Read(path)
                | stdexec::then([&token](std::string content) {
                    if (token.stop_requested())
                    {
                        Log::Info("[Simple] cancelled");
                        return;
                    }
                    Log::Info("[Simple] processing: {}", content);
                })
            );
        });
    }

    ~FileServiceSimple()
    {
        Log::Info("[Simple] dtor — jthread joins + stops automatically");
        // std::jthread dtor calls request_stop() then join() automatically.
        // No extra sync_wait needed here.
    }

private:
    std::jthread _thread;
};

// ---------------------------------------------------------------------------
// Approach 2: run_loop + connect/start + custom Receiver
//
// This is what exec::single_thread_context + exec::async_scope are built on.
// Pro:  No blocking sync_wait inside the service; one thread drives the loop,
//       many operation-states can be enqueued.
// Con:  Must store op-state by type; receiver boilerplate is manual.
// ---------------------------------------------------------------------------

// Helper: make the work sender.
// Declared static so its return type can be used for connect_result_t.
static auto make_work(stdexec::run_loop&             loop,
                      stdexec::inplace_stop_source&  stop,
                      std::string                    path)
{
    return stdexec::starts_on(
        loop.get_scheduler(),
        Read(std::move(path))
            | stdexec::then([&stop](std::string content) {
                if (stop.stop_requested())
                {
                    Log::Info("[Pure] cancelled before processing");
                    return;
                }
                Log::Info("[Pure] processing: {}", content);
            })
    );
}

class FileServicePure
{
    // --- Receiver -----------------------------------------------------------
    // Wires completion back to the service (unlock the dtor wait).
    // Injects stop token into the environment so downstream senders can query it.
    struct Receiver
    {
        using receiver_concept = stdexec::receiver_t;

        FileServicePure* _self;

        void set_value() noexcept          { _self->_notify(); }
        void set_error(auto&&) noexcept    { _self->_notify(); }
        void set_stopped() noexcept        { _self->_notify(); }

        // Provide stop token to the whole sender chain via environment
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

    // --- Op-state holder ----------------------------------------------------
    // Op-states are immovable (__immovable base), so they can't live in
    // std::optional (emplace checks is_constructible<T,T>). The pattern used
    // by start_detached internally: wrap in a heap-allocated struct whose
    // constructor initializes the op-state via member-initializer-list,
    // which uses guaranteed copy elision (C++17) — no move needed.
    using WorkSender = decltype(make_work(std::declval<stdexec::run_loop&>(),
                                         std::declval<stdexec::inplace_stop_source&>(),
                                         std::string{}));

    struct OpHolder
    {
        using Op = stdexec::connect_result_t<WorkSender, Receiver>;
        Op _op;

        // connect() returns a prvalue → direct-initialized here (no move)
        OpHolder(WorkSender&& ws, Receiver r)
            : _op(stdexec::connect(std::move(ws), std::move(r)))
        {}
    };

    // --- Members (order matters for initialization) -------------------------
    stdexec::run_loop            _loop;
    std::jthread                 _thread{[this] { _loop.run(); }};
    stdexec::inplace_stop_source _stop;
    std::mutex                   _mutex;
    std::condition_variable      _cv;
    bool                         _done{false};
    std::unique_ptr<OpHolder>    _op;  // one heap alloc for the op-state

public:
    explicit FileServicePure(std::string path)
    {
        Log::Info("[Pure] ctor, connecting + starting work");
        // make_unique<OpHolder> calls: new OpHolder(work, rcvr)
        // OpHolder ctor: _op(connect(work, rcvr))  ← prvalue, no move
        _op = std::make_unique<OpHolder>(
            make_work(_loop, _stop, std::move(path)),
            Receiver{this}
        );
        stdexec::start(_op->_op);
    }

    ~FileServicePure()
    {
        Log::Info("[Pure] dtor, requesting stop");
        _stop.request_stop();  // inplace_stop_source → propagates into receiver env

        // Wait for the op-state to signal completion (set_value/set_stopped).
        // This is the only sync point — equivalent to async_scope::on_empty().
        std::unique_lock lk{_mutex};
        _cv.wait(lk, [this] { return _done; });

        _loop.finish();  // tell run_loop to exit after queue is drained
        Log::Info("[Pure] dtor, all done");
        // _thread joins automatically (std::jthread)
    }
};

// ---------------------------------------------------------------------------
auto main() -> int
{
    Log::Info("=== Approach 1: jthread + sync_wait in background thread ===");
    {
        FileServiceSimple svc{"config.json"};
    }  // jthread dtor: request_stop() + join()

    Log::Info("=== Approach 1: early destruction (cancel) ===");
    {
        FileServiceSimple svc{"large.dat"};
        // immediately destroyed — jthread requests stop and joins
    }

    Log::Info("=== Approach 2: run_loop + connect/start (pure stdexec) ===");
    {
        FileServicePure svc{"config.json"};
    }  // dtor: request_stop + cv wait + loop.finish + thread join

    Log::Info("=== Approach 2: early destruction (cancel) ===");
    {
        FileServicePure svc{"large.dat"};
    }  // immediately cancelled via inplace_stop_source

    return 0;
}
