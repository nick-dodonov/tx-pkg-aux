#pragma once

namespace Boot
{
    class CliArgs;

    void LogHeader(int argc, const char** argv) noexcept;
    void LogHeader(const CliArgs& args) noexcept;

    void SetupAbortHandlers() noexcept;
    void RestoreAbortHandlers() noexcept;
}
