# pkg/exec

Low-level P2300 scheduling utilities for the update loop.

## Mental model

P2300 (`std::execution`, implemented here by **stdexec**) is an asynchronous programming model built on three primitives:

| Primitive | Represents |
|-----------|------------|
| **Sender** | A description of work — lazy, composable, copyable |
| **Receiver** | The continuation that accepts completion signals: `set_value`, `set_stopped`, `set_error` |
| **OperationState** | The connected, running unit of work produced by `connect(sender, receiver)` |

Work is started by calling `stdexec::start(op)` on the operation state. Senders are composable via algorithms; receivers propagate context through `get_env()`.

---

## UpdateScheduler

`Exec::UpdateScheduler` is a P2300 scheduler for frame-based update loops.

### How it works

1. `scheduler.schedule()` returns a `Sender`.
2. When the sender's operation state is started (`start()`), it **enqueues** the task into a lock-free `ConcurrentQueue`.
3. On each frame, the caller invokes `DrainQueue()`, which dequeues and executes all pending tasks.
4. Each task checks the receiver's stop token: if cancellation was requested it calls `set_stopped()`, otherwise `set_value()`.

### Comparison with standard alternatives

| Scheduler | Execution model | Thread model |
|-----------|----------------|--------------|
| `stdexec::run_loop` | Blocking; waits on a condition variable | Dedicated thread |
| `exec::inline_scheduler` | Immediate; executes inside `start()` | Caller's thread |
| `exec::trampoline_scheduler` | Depth-limited inline execution | Caller's thread |
| **`UpdateScheduler`** | Non-blocking deferred drain | Main/render thread |

`UpdateScheduler` is the right choice when work must run on the main thread at a controlled point (e.g. between physics and render), without blocking the thread waiting for work to arrive.

---

## Key `stdexec::` algorithms

| Algorithm | Purpose |
|-----------|---------|
| `stdexec::starts_on(sched, sndr)` | Start `sndr` on `sched`; used by `Domain::Launch(sender)` |
| `stdexec::continues_on(sndr, sched)` | Transfer completion to `sched`; pipeline step |
| `stdexec::on(sched, sndr, closure)` | Run closure on `sched`, return completion to the current scheduler |
| `stdexec::then(sndr, f)` | Transform `set_value` result inline |
| `stdexec::let_value(sndr, f)` | Chain a new sender from the result; supports async `let_*` |
| `stdexec::when_all(sndrs...)` | Await all senders; first cancellation cancels the rest |
| `stdexec::read_env(query)` | Extract a value from the receiver's environment |
| `stdexec::just(vs...)` | Immediate sender that completes with the given values |

### Accessing the scheduler from inside a coroutine

```cpp
// Inside an exec::task<T> body:
auto sched = co_await stdexec::read_env(stdexec::get_scheduler);
co_await stdexec::schedule(sched);   // reschedule onto the update loop
```

This works because `DomainReceiver::get_env()` exposes `get_scheduler` → `UpdateScheduler::Scheduler`.

---

## Key `exec::` extensions (stdexec extras)

| Utility | Purpose |
|---------|---------|
| `exec::task<T>` | Coroutine sender; returns `T` via `co_return`; stop-token aware |
| `exec::async_scope` | Dynamic lifetime scope; spawn senders without awaiting them immediately |
| `exec::trampoline_scheduler` | Inline scheduler with stack-overflow protection |
| `exec::create(f)` | Factory for building custom senders from a callback |
| `exec::start_now(sndr)` | Start a sender immediately in a detached scope |
| `exec::scope` | RAII wrapper requiring explicit `join()` before destruction |

### `exec::task<int>` example

```cpp
exec::task<int> MainTask()
{
    // Runs on UpdateScheduler (scheduler propagated via DomainReceiver::get_env())
    co_await stdexec::schedule(co_await stdexec::read_env(stdexec::get_scheduler));
    co_return 42;
}

// In setup:
domain->Launch(MainTask());     // starts_on(scheduler, MainTask())
```

---

## `starts_on` vs factory `Launch()`

```cpp
// Form 1: sender — Domain calls starts_on internally
domain->Launch(MainTask());

// Form 2: factory — receives the scheduler to build the pipeline
domain->Launch([](auto sched) {
    return stdexec::schedule(sched)
        | stdexec::then([] { return 42; });
});
```

Use form 2 when steps of the pipeline need the scheduler handle to insert `continues_on` transfers or to spawn nested work via `exec::async_scope`.

---

## Stop-token propagation

`Domain::_stopSource` owns a `stdexec::inplace_stop_source`. Its token is exposed through `DomainReceiver::get_env()` under `stdexec::get_stop_token`. When `Domain::Stop()` is called:

1. `_stopSource.request_stop()` signals cancellation to all stop-token observers.
2. `_opState.reset()` destroys the operation state.

Stop-token-aware senders (`exec::task`, custom `Operation` structs that check `stopToken.stop_requested()`) will unwind cleanly between drain cycles rather than being destroyed mid-execution.
