# pkg/exec

Low-level P2300 scheduling primitives. See [README.Domain.md](../README.Domain.md) for the `Domain` integration with `RunLoop`.
- [User’s Guide about `stdexec`](https://nvidia.github.io/stdexec/user/index.html).

---

## P2300 mental model (`std::execution`)

P2300 is an asynchronous programming model built on three primitives:

| Primitive | Represents |
|-----------|------------|
| **Sender** | A lazy description of work — composable, copyable, not yet started |
| **Receiver** | The continuation: accepts `set_value`, `set_stopped`, or `set_error` signals |
| **OperationState** | The live, connected unit produced by `connect(sender, receiver)` |

Key properties:
- `connect(sndr, rcvr)` → op state (non-movable after construction)
- `start(op)` initiates execution — the only call that has side effects
- Receivers propagate context (scheduler, stop token, ...) upward via `get_env()`
- Senders query that context downward via `stdexec::read_env(query)`

---

## Key `stdexec::` algorithms

| Algorithm | Completion signals | Purpose |
|-----------|-------------------|---------|
| `stdexec::just(vs...)` | `set_value(vs...)` | Immediate value sender |
| `stdexec::just_stopped()` | `set_stopped()` | Immediate cancellation sender |
| `stdexec::schedule(sched)` | `set_value()`, `set_stopped()` | Enqueue on scheduler, check stop token on resume |
| `stdexec::starts_on(sched, sndr)` | inherits `sndr` | Start `sndr` on `sched`; transfers execution context |
| `stdexec::continues_on(sndr, sched)` | inherits `sndr` | Transfer completion of `sndr` to `sched` |
| `stdexec::on(sched, sndr, closure)` | inherits `closure` | Run `closure(sndr)` on `sched`, return to original scheduler |
| `stdexec::then(sndr, f)` | `set_value(f(vs...))` | Transform value result synchronously |
| `stdexec::upon_stopped(sndr, f)` | `set_value(f())` | Handle cancellation, return a value |
| `stdexec::let_value(sndr, f)` | inherits `f(vs...)` | Chain a new sender from the value |
| `stdexec::let_stopped(sndr, f)` | inherits `f()` | Chain a new sender on cancellation |
| `stdexec::when_all(sndrs...)` | `set_value(all...)`, `set_stopped()` | Await all; first stop cancels the rest |
| `stdexec::stop_when(sndr, trigger)` | inherits `sndr` | Cancel `sndr` when `trigger` fires |
| `stdexec::read_env(query)` | `set_value(query(env))` | Extract a value from the receiver environment |
| `stdexec::with_env(sndr, env)` | inherits `sndr` | Attach a custom environment to a sender |

### Reading context from inside a coroutine

```cpp
// exec::task<T> body — get_scheduler and get_stop_token come from the receiver's env
auto sched = co_await stdexec::read_env(stdexec::get_scheduler);
auto token = co_await stdexec::read_env(stdexec::get_stop_token);

co_await stdexec::schedule(sched);   // hop onto the scheduler
if (token.stop_requested()) { ... }
```

---

## Key `exec::` extensions

| Utility | Purpose |
|---------|---------|
| `exec::task<T>` | Coroutine sender; `co_return T`; stop-token aware; sticky-scheduler |
| `exec::async_scope` | Dynamic lifetime scope; `spawn(sndr)` without awaiting immediately; `join()` to drain |
| `exec::counting_scope` | Like `async_scope` but with reference counting; nests naturally |
| `exec::trampoline_scheduler` | Depth-limited inline scheduler; prevents stack overflow in recursive chains |
| `exec::create(f)` | Build a custom sender from a callback without writing a full sender type |
| `exec::start_now(sndr)` | Start a sender immediately in a detached scope (fire-and-forget) |
| `exec::scope` | RAII scope requiring explicit `join()` before destruction |
| `exec::system_scheduler` | System thread-pool scheduler (platform-provided) |

### `exec::task<T>` sticky-scheduler behaviour

`exec::task` is "scheduler-sticky": after each `co_await`, if the awaited sender completes on a different scheduler than the one in `get_env(rcvr)`, the task automatically inserts a `continues_on` hop back. This means coroutines always resume on the scheduler they were started on, without manual `continues_on` calls.

---

## PureLoopContext

`Exec::PureLoopContext` is a P2300 scheduler for frame-based update loops.

### How it works

1. `scheduler.schedule()` returns a `Sender`.
2. `start(op)` enqueues an `Operation<R>*` into a lock-free `ConcurrentQueue` (never executes inline).
3. The caller invokes `DrainQueue()` once per frame; it dequeues and executes all pending entries.
4. Each entry checks the receiver stop token: `stop_requested()` → `set_stopped()`, else → `set_value()`.

`DrainQueue()` is **greedy**: tasks spawned during a drain (e.g. from inside a coroutine hop) are also dequeued in the same call. A multi-hop `exec::task` therefore typically completes within a single frame.

### Comparison with standard alternatives

| Scheduler | Execution model | Thread model |
|-----------|----------------|--------------|
| `stdexec::run_loop` | Blocking; waits on a condition variable | Dedicated thread |
| `exec::inline_scheduler` | Immediate; executes inside `start()` | Caller's thread |
| `exec::trampoline_scheduler` | Depth-limited inline execution | Caller's thread |
| `exec::system_scheduler` | Thread-pool, platform-managed | Worker threads |
| **`PureLoopContext`** | Non-blocking deferred drain | Main/render thread |

Use `PureLoopContext` when work must run on the main thread at a controlled frame boundary, without blocking while waiting for work to arrive.

