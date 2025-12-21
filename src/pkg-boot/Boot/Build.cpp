#include "Build.h"
#include <sstream>

namespace Build
{
    std::string BuildDescription()
    {
        std::ostringstream desc;

// Build configuration
#ifdef NDEBUG
        desc << "Release";
#else
        desc << "Debug";
#endif

        // Platform information
        desc << " | ";
#if defined(__wasm__) || defined(__EMSCRIPTEN__)
        desc << "WebAssembly";
    #ifdef __EMSCRIPTEN__
        desc << " (Emscripten)";
    #endif
#elif defined(_WIN32) || defined(_WIN64)
        desc << "Windows";
    #ifdef _WIN64
        desc << " x64";
    #else
        desc << " x86";
    #endif
#elif defined(__APPLE__)
        desc << "macOS";
    #if defined(__arm64__) || defined(__aarch64__)
        desc << " ARM64";
    #elif defined(__x86_64__)
        desc << " x64";
    #endif
#elif defined(__linux__)
        desc << "Linux";
    #if defined(__arm64__) || defined(__aarch64__)
        desc << " ARM64";
    #elif defined(__x86_64__)
        desc << " x64";
    #elif defined(__i386__)
        desc << " x86";
    #endif
#else
        desc << "Unknown Platform";
#endif

        // Architecture information
        desc << " | ";
#if defined(__aarch64__) || defined(_M_ARM64)
        desc << "ARM64";
#elif defined(__arm__) || defined(_M_ARM)
        desc << "ARM32";
#elif defined(__x86_64__) || defined(_M_X64) || defined(__amd64__)
        desc << "x64";
#elif defined(__i386__) || defined(_M_IX86) || defined(__i386)
        desc << "x86";
#elif defined(__riscv) && (__riscv_xlen == 64)
        desc << "RISC-V 64";
#elif defined(__riscv) && (__riscv_xlen == 32)
        desc << "RISC-V 32";
#else
        desc << "(Unknown)";
#endif

        // Compiler information
        desc << " | ";
#if defined(__clang__)
        desc << "Clang " << __clang_major__ << "." << __clang_minor__ << "." << __clang_patchlevel__;
#elif defined(__GNUC__)
        desc << "GCC " << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__;
#elif defined(_MSC_VER)
        desc << "MSVC " << _MSC_VER;
#else
        desc << "Unknown Compiler";
#endif

        // C++ standard
        desc << " | C++";
#if __cplusplus >= 202300L
        desc << "26";
#elif __cplusplus >= 202002L
        desc << "20";
#elif __cplusplus >= 201703L
        desc << "17";
#elif __cplusplus >= 201402L
        desc << "14";
#elif __cplusplus >= 201103L
        desc << "11";
#else
        desc << "Pre-11";
#endif

        return desc.str();
    }
}