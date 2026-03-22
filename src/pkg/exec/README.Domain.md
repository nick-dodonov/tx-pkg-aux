# pkg/app-exec

`Exec::Domain` — an `RunLoop::Handler` that drives a P2300 sender pipeline on a `TimedLoopContext`. See [pkg/exec](../exec/README.md) for the P2300 mental model, available `stdexec::` algorithms, and `exec::` extensions.

---

## Domain

`Domain` bridges the P2300 sender/receiver model with the `RunLoop` update cycle:

- Construct with a sender or factory → stores the connected op state
- `Start()` calls `stdexec::start(op)` — the first task is enqueued on the scheduler
- `Update()` drains the scheduler queue (once per frame)
- When the sender completes with `int`, the runner exits with that code
- `Stop()` signals stop, drains pending entries, then destroys the op state

### Constructors

```cpp
// Form 1: direct sender — Domain wraps it with starts_on(scheduler, sender)
auto domain = std::make_shared<Domain>(MainTask());
auto domain = std::make_shared<Domain>(stdexec::just(42));

// Form 2: factory (Scheduler) → sender — use when the pipeline needs the scheduler handle
auto domain = std::make_shared<Domain>([](auto sched) {
    return stdexec::schedule(sched)
        | stdexec::then([] { return 42; });
});
```

Use form 2 when pipeline steps need the scheduler to insert `continues_on` transfers or to spawn nested work via `exec::async_scope`.

### Example with `exec::task`

```cpp
exec::task<int> MainTask()
{
    // Scheduler is available via read_env because DomainReceiver::get_env()
    // exposes get_scheduler → TimedLoopContext::Scheduler.
    const auto sched = co_await stdexec::read_env(stdexec::get_scheduler);
    co_await stdexec::schedule(sched);   // hop to the next drain cycle
    co_return 42;
}

// In main:
auto domain = std::make_shared<Domain>(MainTask());
auto runner = App::CreateDefaultRunner(domain);
return runner->Run();
```

---

## Stop-token propagation

`Domain` owns an `stdexec::inplace_stop_source`. Its token is exposed to the running sender via `DomainReceiver::get_env()` under `stdexec::get_stop_token`.

When `Domain::Stop()` is called:

1. `_stopSource.request_stop()` — sets the atomic stop flag; all observers see it immediately.
2. `_scheduler.DrainQueue()` — executes any tasks already in the queue so they can observe `stop_requested()` and call `set_stopped()` cleanly, *before* the op state is destroyed.
3. `_opState.reset()` — destroys the op state; at this point the queue is empty and there are no dangling `Operation<R>*` pointers.

Stop-token-aware senders (`exec::task`, any custom sender that checks the token in its execute callback) unwind gracefully rather than being destroyed mid-flight. A sender that ignores the stop token will have `set_stopped()` called on it at drain time instead of `set_value()`.

---

## DomainReceiver environment

`DomainReceiver::get_env()` returns a composed environment exposing two queries:

| Query | Value | Effect |
|-------|-------|--------|
| `stdexec::get_scheduler` | `TimedLoopContext::Scheduler` | `co_await read_env(get_scheduler)` works inside `exec::task`; `stdexec::on` / `continues_on` can chain back to the loop |
| `stdexec::get_stop_token` | `inplace_stop_token` | Cancellation propagates into the pipeline; `exec::task` observes it automatically |

Without `get_env()` (or with `empty_env{}`), stop-token propagation and scheduler context are both silently broken — the sender executes without a scheduler in scope and can never observe cancellation between drain cycles.
