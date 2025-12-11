#include "Log/Log.h"

//TODO: add samples w/ Src modifiers (NoFunc)

static void FuncDemo()
{
    Log::Trace("func message (trace)");
    Log::Debug("func message (debug)");
    Log::Info("func message (info)");
    Log::Warn("func message (warning)");
    Log::Error("func message (error)");
    Log::Fatal("func message (fatal)");
    Log::Debug("func formatted: '{}'", "demo");
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

static void LoggerFuncDemo()
{
    Log::Logger logger("LoggerFunc");
    logger.Trace("logger func message (trace)");
    logger.Debug("logger func message (debug)");
    logger.Info("logger func message (info)");
    logger.Warn("logger func message (warning)");
    logger.Error("logger func message (error)");
    logger.Fatal("logger func message (fatal)");
    logger.Debug("logger func formatted: '{}'", "demo");
}

static void LoggerMacroDemo()
{
    Log::Logger logger("LoggerMacro");
    LogTrace(logger, "logger macro msg (trace)");
    LogDebug(logger, "logger macro msg (debug)");
    LogInfo(logger, "logger macro msg (info)");
    LogWarn(logger, "logger macro msg (warn)");
    LogError(logger, "logger macro msg (error)");
    LogFatal(logger, "logger macro msg (fatal)");
    LogDebug(logger, "logger macro formatted: {}", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
}

struct FeatureDemo
{
    Log::Logger logger{"FeatureArea"};

    static Log::Logger& DefaultLogger()
    {
        static Log::Logger logger("FeatureDefault");
        return logger;
    }

    // ReSharper disable once CppMemberFunctionMayBeStatic
    void DefaultMacroDemo()
    {
        LogTrace("feature msg (trace)");
        LogDebug("feature msg (debug)");
        LogInfo("feature msg (info)");
        LogWarn("feature msg (warn)");
        LogError("feature msg (error)");
        LogFatal("feature msg (fatal)");
        LogDebug("feature formatted: {}", -12345);
    }
};

int main(const int argc, char**)
{
    FuncDemo();
    MacroDemo();
    LoggerFuncDemo();
    LoggerMacroDemo();
    FeatureDemo feature;
    feature.DefaultMacroDemo();

    return argc - 1; // to check exit code is passed from emrun
}
