# Capy Coroutine Execution Examples

This directory contains examples of running Boost.Capy coroutines with different execution strategies.

## Examples

### 1. `main_pool.cpp` - Thread Pool Execution
The original example using `capy::thread_pool` to run coroutines on a background thread.

**Build:** `bazel build //demo/try/capy:main_pool`

### 2. `main_loop.cpp` - Main Thread Event Loop
Demonstrates running coroutines on the main thread using a `run_loop` executor.

**Key Features:**
- Single-threaded execution context (no thread pool)
- Event loop pattern similar to GUI frameworks
- All coroutines execute on the main thread
- Coroutines are queued and executed when `loop.run()` is called

**Build:** `bazel build //demo/try/capy:main_loop`

### 3. `main_inline.cpp` - Immediate Inline Execution
The simplest possible executor that executes coroutines immediately without any queuing.

**Key Features:**
- No thread pool or event loop
- Coroutines execute synchronously and inline
- No context switching
- Useful for testing or deterministic execution

**Build:** `bazel build //demo/try/capy:main_inline`

## Executor Comparison

| Executor Type | Thread Pool | Queuing | Async | Use Case |
|---------------|-------------|---------|-------|----------|
| `thread_pool` | Yes | Yes | Yes | Multi-threaded async operations |
| `run_loop` | No | Yes | No | Single-threaded event loop (GUI, game loop) |
| `inline_executor` | No | No | No | Synchronous testing, deterministic execution |

## Custom Executor Implementation

To create a custom executor without a thread pool:

1. **Inherit from `capy::execution_context`**
   ```cpp
   class my_executor : public capy::execution_context {
       // ...
   };
   ```

2. **Implement `executor_type` nested class** with these methods:
   - `context()` - returns the execution context
   - `dispatch(handle)` - execute immediately or queue for later
   - `post(handle)` - always queue/defer execution
   - `on_work_started()` / `on_work_finished()` - lifecycle hooks

3. **Satisfy the `capy::Executor` concept**
   ```cpp
   static_assert(capy::Executor<my_executor::executor_type>);
   ```

## Running the Examples

### With Bazel:
```bash
# Thread pool example
bazel run //demo/try/capy:main_pool

# Main thread event loop
bazel run //demo/try/capy:main_loop

# Inline executor
bazel run //demo/try/capy:main_inline
```

### Expected Output

All examples will show thread IDs to demonstrate where coroutines execute:

- **Thread pool**: Different thread ID from main
- **Run loop**: Same thread ID as main (after event loop starts)
- **Inline**: Same thread ID as main throughout

## References

- [Capy GitHub Repository](https://github.com/cppalliance/capy)
- [Capy Custom Executor Example](https://github.com/cppalliance/capy/tree/develop/example/custom-executor)
- [Capy Documentation](https://develop.capy.cpp.al/)

## Answer to the Question

**Yes**, there is an executor without a thread pool for running coroutines on the main thread:

1. **`run_loop`** - Best choice for event-driven applications (GUI, game loops)
   - Queues coroutines and executes them when you call `run()`
   - Similar to `QEventLoop` in Qt or `NSRunLoop` in macOS

2. **`inline_executor`** - For immediate synchronous execution
   - No queuing, executes immediately
   - Good for testing or when you need deterministic execution order

Both run entirely on the calling thread without spawning new threads.
