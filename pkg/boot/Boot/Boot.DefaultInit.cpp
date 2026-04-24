#include "Boot.h"
#include "Log/Log.h"

namespace Boot
{
    static bool _inited = false;
    static CliArgs _args;

    const CliArgs& GetDefaultArgs() noexcept
    {
        return _args;
    }

    CliArgs DefaultInit(int argc, const char** argv) noexcept
    {
        if (_inited) {
            return _args;
        }
        _args = CliArgs(argc, argv);
        Log::RegisterThread("<main>");
        LogHeader(_args);
        SetupAbortHandlers();
        _inited = true;
        return _args;
    }
}
