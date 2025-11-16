#include "Log/Log.h"

static void FuncDemo()
{
    Log::Trace("func message (trace)");
    Log::Debug("func message (debug)");
    Log::Info("func message (info)");
    Log::Warn("func message (warning)");
    Log::Error("func message (error)");
    Log::Fatal("func message (fatal)");
    Log::Debug("func formatted: {} ''", _LIBCPP_STD_VER, "demo");
}

static void MacroDemo()
{
    LogTrace("macro msg (trace)");
    LogDebug("macro msg (debug)");
    LogInfo("macro msg (info)");
    LogWarn("macro msg (warn)");
    LogError("macro msg (error)");
    LogFatal("macro msg (fatal)");
    LogDebug("macro formatted: {}", 12345);
}

struct FeatureDemo
{
    void DoSomething()
    {
        LogTrace("feature msg (trace)");
        LogDebug("feature msg (debug)");
    }
};

int main(int argc, char* argv[])
{
    FuncDemo();
    MacroDemo();
    FeatureDemo feature;
    feature.DoSomething();

    Log::Info("Command:");
    for (int i = 0; i < argc; ++i) {
        Log::Info("  Arg[{}]: {}", i, argv[i]);
    }

    auto exitCode = argc - 1;
    Log::Info("Exit: {}", exitCode);

    return exitCode; // to check exit code is passed from emrun
}
