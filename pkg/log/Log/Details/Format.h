#pragma once
#include <memory>
#include <string>

namespace spdlog
{
    class formatter;
}

namespace Log::Detail
{
    std::unique_ptr<spdlog::formatter> MakeDefaultFormatter();

    /// Register a human-readable name for the calling thread.
    /// Thread-safe; may be called from any thread at any time.
    void RegisterThread(std::string name);

    /// Returns the registered name for the calling thread, or empty string if none.
    std::string GetCurrentThreadName();

    /// Returns the numeric OS thread ID for the calling thread.
    size_t GetCurrentThreadId() noexcept;
}
