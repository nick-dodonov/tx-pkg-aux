#include "Boot.h"

#include <exception>
#include <print>
#include <cstdio>

#if !__EMSCRIPTEN__
    #include <array>
    #include <csignal>
    #if _WIN32
        #include <windows.h>
        #include <dbghelp.h>
        #include <io.h>
    #else
        #include <execinfo.h>
        #include <unistd.h>
    #endif
#endif

namespace Boot
{
    static std::terminate_handler _prev_terminate_handler;

    static void print_stack_trace()
    {
#if !__EMSCRIPTEN__
    #if _WIN32
        HANDLE process = GetCurrentProcess();
        HANDLE thread = GetCurrentThread();
        
        CONTEXT context;
        memset(&context, 0, sizeof(CONTEXT));
        context.ContextFlags = CONTEXT_FULL;
        RtlCaptureContext(&context);
        
        SymInitialize(process, NULL, TRUE);
        
        DWORD image_type;
        STACKFRAME64 stackframe;
        ZeroMemory(&stackframe, sizeof(STACKFRAME64));
        
        #ifdef _M_IX86
            image_type = IMAGE_FILE_MACHINE_I386;
            stackframe.AddrPC.Offset = context.Eip;
            stackframe.AddrPC.Mode = AddrModeFlat;
            stackframe.AddrFrame.Offset = context.Ebp;
            stackframe.AddrFrame.Mode = AddrModeFlat;
            stackframe.AddrStack.Offset = context.Esp;
            stackframe.AddrStack.Mode = AddrModeFlat;
        #elif _M_X64
            image_type = IMAGE_FILE_MACHINE_AMD64;
            stackframe.AddrPC.Offset = context.Rip;
            stackframe.AddrPC.Mode = AddrModeFlat;
            stackframe.AddrFrame.Offset = context.Rsp;
            stackframe.AddrFrame.Mode = AddrModeFlat;
            stackframe.AddrStack.Offset = context.Rsp;
            stackframe.AddrStack.Mode = AddrModeFlat;
        #elif _M_ARM64
            image_type = IMAGE_FILE_MACHINE_ARM64;
            stackframe.AddrPC.Offset = context.Pc;
            stackframe.AddrPC.Mode = AddrModeFlat;
            stackframe.AddrFrame.Offset = context.Fp;
            stackframe.AddrFrame.Mode = AddrModeFlat;
            stackframe.AddrStack.Offset = context.Sp;
            stackframe.AddrStack.Mode = AddrModeFlat;
        #endif
        
        for (size_t frame_num = 0; frame_num < 128; ++frame_num) {
            BOOL result = StackWalk64(
                image_type,
                process,
                thread,
                &stackframe,
                &context,
                NULL,
                SymFunctionTableAccess64,
                SymGetModuleBase64,
                NULL);
                
            if (!result) {
                break;
            }
            
            char symbol_buffer[sizeof(SYMBOL_INFO) + 256];
            SYMBOL_INFO* symbol_info = reinterpret_cast<SYMBOL_INFO*>(symbol_buffer);
            symbol_info->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol_info->MaxNameLen = 255;
            
            DWORD64 symbol_displacement = 0;
            if (SymFromAddr(process, stackframe.AddrPC.Offset, &symbol_displacement, symbol_info)) {
                fprintf(stderr, "  Frame %zu: %s + 0x%llx\n", frame_num, symbol_info->Name, symbol_displacement);
            } else {
                fprintf(stderr, "  Frame %zu: 0x%llx\n", frame_num, stackframe.AddrPC.Offset);
            }
        }
        
        SymCleanup(process);
    #else
        std::array<void*, 128> callstack{};
        const auto frames = backtrace(callstack.data(), callstack.size());
        backtrace_symbols_fd(callstack.data(), frames, STDERR_FILENO);
    #endif
#endif
    }

    static void terminate_handler()
    {
        const auto ptr = std::current_exception();
        fprintf(stderr, "\n=== Boot: terminate captured (%s) ===\n", ptr ? "exception active" : "no exception");
        print_stack_trace();

        RestoreAbortHandlers();
        std::abort();
    }

#if !__EMSCRIPTEN__
    static void sigabrt_handler(int sig)
    {
        fprintf(stderr, "\n=== Boot: SIGABRT %d captured ===\n", sig);
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
