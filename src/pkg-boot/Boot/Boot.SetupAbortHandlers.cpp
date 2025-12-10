#include "Boot.h"

#include <exception>
#include <print>

#if !__EMSCRIPTEN__
    #include <array>
    #include <csignal>
    #include <execinfo.h>
    #include <unistd.h>
#endif

namespace Boot
{
    static std::terminate_handler _prev_terminate_handler;

    static void print_stack_trace()
    {
#if !__EMSCRIPTEN__
        std::array<void*, 128> callstack{};
        const auto frames = backtrace(callstack.data(), callstack.size());
        backtrace_symbols_fd(callstack.data(), frames, STDERR_FILENO);
#endif
    }

    static void terminate_handler()
    {
        const auto ptr = std::current_exception();
        std::println(stderr, "\n=== Boot: terminate captured ({}) ===", ptr ? "exception active" : "no exception");
        print_stack_trace();

        RestoreAbortHandlers();
        std::abort();
    }

#if !__EMSCRIPTEN__
    static void sigabrt_handler(int sig)
    {
        std::println(stderr, "\n=== Boot: SIGABRT {} captured ===", sig);
        print_stack_trace();

        RestoreAbortHandlers();
        std::abort();
    }
#endif

    void SetupAbortHandlers() noexcept
    {
        _prev_terminate_handler = std::get_terminate();
        std::set_terminate(terminate_handler);

#if !__EMSCRIPTEN__
        std::signal(SIGABRT, sigabrt_handler);
#endif
    }

    void RestoreAbortHandlers() noexcept
    {
#if !__EMSCRIPTEN__
        std::signal(SIGABRT, SIG_DFL);
#endif

        std::set_terminate(_prev_terminate_handler);
        _prev_terminate_handler = nullptr;
    }
};
