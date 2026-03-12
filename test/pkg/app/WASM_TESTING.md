# WASM Testing Solutions

## Problem

Testing coroutine-based code with `emscripten_set_main_loop_arg(simulate_infinite_loop=true)` fails because:
- The function never returns (by design for browser event loop compatibility)
- Google Test expects `main()` to return with an exit code
- Tests hang indefinitely when using `WasmRunner`

## ✅ Solution: Use TightRunner for Tests (Implemented)

**Status**: ✅ Working - tests pass on both native and WASM

Use existing `TightRunner` which:
- Polls `io_context` directly without entering emscripten's event loop
- Returns normally when work completes
- Works identically on all platforms (WASM, macOS, Linux)
- Already existed for performance benchmarks and unlimited frame rate scenarios

### Usage in Tests:
```cpp
#include "App/Loop/TightRunner.h"

auto domain = std::make_shared<App::Domain>();
auto runner = std::make_shared<App::Loop::TightRunner>(domain);
int exitCode = domain->RunCoroMain(runner, coroMain());
```

### Why TightRunner Works for Tests:
- Synchronous execution model - compatible with gtest expectations
- No dependency on browser event loop
- Identical behavior across platforms
- Already battle-tested for tight loop scenarios

### Advantages:
- ✅ No new code needed - reuse existing TightRunner
- ✅ Works on all platforms without platform-specific code
- ✅ Tests are portable and predictable
- ✅ No special emscripten flags or workarounds needed
- ✅ Simple to use - just instantiate TightRunner

### Disadvantages:
- ⚠️ Tests don't use the real production WasmRunner (different execution path)
- ⚠️ Won't catch WasmRunner-specific issues
- ℹ️ Note: Integration tests in actual browser environment should use real WasmRunner

---

## Alternative Solution 2: Asyncify with emscripten_sleep

**Status**: ⚠️ Not implemented (would add overhead)

Use Emscripten's Asyncify feature to yield control back to browser between iterations.

### Implementation:
```cpp
#if __EMSCRIPTEN__
    #include <emscripten.h>
    
    // In test's main loop
    while (running) {
        updateCtx.Tick();
        InvokeUpdate(updateCtx);
        emscripten_sleep(0);  // Yield to browser
    }
#endif
```

### Build Flags:
```bash
--copt=-s --copt=ASYNCIFY
--linkopt=-s --linkopt=ASYNCIFY
```

### Advantages:
- ✅ Allows synchronous-looking code to work in browser
- ✅ Can return from functions normally

### Disadvantages:
- ❌ Significant size overhead (~15-30%)
- ❌ Performance overhead
- ❌ Requires special build flags
- ❌ More complex to set up

**Reference**: https://emscripten.org/docs/porting/asyncify.html

---

## Alternative Solution 3: Platform-Specific Test Skipping

**Status**: ⚠️ Not recommended (reduces test coverage)

Skip tests that require main loop on WASM:

```cpp
TEST(DomainTest, RunSimpleCoroMain)
{
#if __EMSCRIPTEN__
    GTEST_SKIP() << "Skipped on WASM - requires event loop";
#else
    auto domain = std::make_shared<App::Domain>();
    auto runner = App::Loop::CreateDefaultRunner(domain);
    int exitCode = domain->RunCoroMain(runner, coroMain());
    EXPECT_EQ(exitCode, 42);
#endif
}
```

### Advantages:
- ✅ Simple to implement
- ✅ No code changes needed

### Disadvantages:
- ❌ Reduces test coverage on WASM
- ❌ Defeats the purpose of cross-platform testing
- ❌ Can hide WASM-specific bugs

---

## Alternative Solution 4: Mock/Stub Runner

**Status**: ⚠️ Not implemented (similar to Solution 1 but more complex)

Create a mock runner for testing that doesn't actually run a loop:

```cpp
class MockRunner : public IRunner {
public:
    int Run() override {
        // Don't run any loop, just call handler once
        handler->Start();
        handler->Update(UpdateCtx{*this});
        handler->Stop();
        return exitCode;
    }
    
    void Exit(int code) override { exitCode = code; }
private:
    int exitCode = 0;
};
```

### Advantages:
- ✅ Very simple
- ✅ Fast tests

### Disadvantages:
- ❌ Doesn't test real async behavior
- ❌ Coroutines might not complete properly
- ❌ Less useful than TestRunner

---

## Alternative Solution 5: EXIT_RUNTIME Flag

**Status**: ⚠️ Not working (emscripten still doesn't return)

Try forcing runtime exit with emscripten flags:

```bash
bazel test //test:app --config=wasm \
  --copt=-s --copt=EXIT_RUNTIME=1 \
  --linkopt=-s --linkopt=EXIT_RUNTIME=1
```

### Why It Doesn't Work:
- `emscripten_set_main_loop_arg` with `simulate_infinite_loop=true` prevents normal exit by design
- Even with `EXIT_RUNTIME=1`, control never returns to test framework
- Would need to change `simulate_infinite_loop` to `false`, which breaks the event loop model

---

## Recommendation

**Use TightRunner** because:

1. ✅ **Already exists** - no new code to maintain
2. ✅ **Platform agnostic** - same code on all platforms
3. ✅ **Proven solution** - already used for benchmarks
4. ✅ **Tests real async behavior** - coroutines execute properly
5. ✅ **No overhead** - no Asyncify or special flags needed

The trade-off of not using `WasmRunner` in tests is acceptable because:
- Production code still uses `WasmRunner`
- Unit tests focus on Domain/coroutine logic, not runner implementation
- Integration tests in actual browser environment would use real `WasmRunner`
- `TightRunner` tests the same async execution model, just without browser event loop integration

---

## Test Results

### Native (macOS):
```
bazel test //test/app:app --test_output=all
[==========] 7 tests from 1 test suite ran.
[  PASSED  ] 7 tests.
```

### WASM:
```
bazel test //test/app:app --config=wasm --test_output=all
[==========] 7 tests from 1 test suite ran.
[  PASSED  ] 7 tests.
✅ Success: 0
```

Both platforms pass all tests! 🎉

---

## Additional Notes

### Why emscripten_set_main_loop never returns

From Emscripten documentation:
> The browser event model uses co-operative multitasking — each event has a "turn" to run, and must then return control to the browser. JavaScript APIs like WebGL can only run when the current "turn" is over.

The `simulate_infinite_loop` parameter:
- `true`: Emscripten simulates infinite loop by throwing exception to unwind stack (never returns)
- `false`: Function returns immediately, but callbacks won't execute properly

### Alternative Event Loop APIs

Emscripten provides several main loop APIs:
- `emscripten_set_main_loop()` - older API
- `emscripten_set_main_loop_arg()` - with user data (used in WasmRunner)
- `emscripten_request_animation_frame_loop()` - newer, recommended for rendering
- `emscripten_async_call()` - single delayed call

All have the same "never returns" behavior when used correctly.

---

## Future Improvements

1. **Use existing TightRunner** ✅ Done
2. **Browser integration tests** - run full WasmRunner in headless browser
3. **Performance benchmarks** - compare different runners timing
4. **CI/CD integration** - automate WASM testing in pipeline
