#pragma once
#include <Boot/CliArgs.h>

namespace Boot
{
    CliArgs DefaultInit(int argc, const char** argv) noexcept;
    //CliArgs DefaultInit(int argc, char** argv) noexcept  { return DefaultInit(argc, const_cast<const char**>(argv)); }

    void LogHeader(const CliArgs& args) noexcept;

    void SetupAbortHandlers() noexcept;
    void RestoreAbortHandlers() noexcept;
}
