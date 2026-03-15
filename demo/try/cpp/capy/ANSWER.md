# Capy Coroutine Executors - Summary

## Question
How to run Capy coroutines on the main thread? Is there an executor without a thread pool?

## Answer
**Yes**, Capy provides multiple ways to run coroutines on the main thread without a thread pool:

### 1. Run Loop Executor (`run_loop`)
A single-threaded execution context with an event loop pattern:
- Queues coroutine handles
- Executes them when you call `run()`
- Perfect for GUI applications, game loops, or any event-driven architecture

**Example:** [main_loop.cpp](main_loop.cpp)

```cpp
run_loop loop;
capy::run_async(loop.get_executor())(my_coroutine());
loop.run(); // Drains the queue on the main thread
```

### 2. Inline Executor (`inline_executor_context`)
Executes coroutines immediately without any queuing:
- No event loop needed
- Fully synchronous execution
- Useful for testing or deterministic behavior

**Example:** [main_inline.cpp](main_inline.cpp)

```cpp
inline_executor_context ctx;
capy::run_async(ctx.get_executor())(my_coroutine());
// Coroutines complete synchronously - no run() needed
```

## Implementation Details

Both executors implement the `capy::Executor` concept by:

1. Inheriting from `capy::execution_context`
2. Providing an `executor_type` nested class with:
   - `dispatch(handle)` - execute immediately or queue
   - `post(handle)` - defer execution
   - `context()` - return the execution context

The key difference:
- **run_loop**: `dispatch()` queues if not on the correct thread
- **inline_executor**: `dispatch()` always returns the handle for immediate execution

## Build and Run

```bash
# Main thread with event loop
bazel run //demo/try/capy:main_loop

# Inline executor
bazel run //demo/try/capy:main_inline

# Original thread pool example
bazel run //demo/try/capy:main_pool
```

## Output Verification

All examples log the thread ID to prove execution stays on the main thread:
- Thread pool: Different thread ID
- Run loop: Same thread ID as main (0x1f626f100 in test run)
- Inline executor: Same thread ID as main (0x1f626f100 in test run)

## References

- [Capy Custom Executor Example](https://github.com/cppalliance/capy/tree/develop/example/custom-executor)
- [Capy GitHub Repository](https://github.com/cppalliance/capy)
- Full documentation in [README.md](README.md)
