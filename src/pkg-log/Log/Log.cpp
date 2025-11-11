#include "Log.h"
#include <lwlog.h>

namespace Log {
    using Logger = lwlog::console_logger;
    static std::shared_ptr<Logger> g_logger;

    void DefaultInit()
    {
        g_logger = std::make_shared<Logger>("default");
        // g_logger->set_level_filter(lwlog::level::info | lwlog::level::debug | lwlog::level::critical);

        // TODO: check/fix in web-tools and emrun
        // g_logger->set_pattern("[%T] [%n] [%l]: %v");
        // g_logger->set_pattern("{file} .red([%T] [%n]) .dark_green([:^12{level}]): .cyan(%v) TEXT");

        // https://github.com/ChristianPanov/lwlog/tree/v1.4.0?tab=readme-ov-file#syntax
#if defined(__EMSCRIPTEN__)
    #define SUBSECS "%e" // wasm precision isn't good
#else
    #define SUBSECS "%f" // microseconds is enough for native
#endif
        // TODO: {topic} supported
        // TODO: fix bug adding '.' in time crashes (so '·' is used instead)
        // TODO: fix time in emscripten (it shows UTC, not local time)
        // TODO: {thread} depending on platform
        g_logger->set_pattern("[%T·" SUBSECS "] %t :<8%l [%n] %v");

        // g_logger->debug("Logger initialized");
    }

    void Debug(const std::string& msg)
    {
        if (!g_logger) {
            DefaultInit();
        }
        g_logger->debug(msg.c_str());
    }

    void Info(const std::string& msg)
    {
        if (!g_logger) {
            DefaultInit();
        }
        g_logger->info(msg.c_str());
    }

    void Warn(const std::string& msg)
    {
        if (!g_logger) {
            DefaultInit();
        }
        g_logger->warning(msg.c_str());
    }

    void Error(const std::string& msg)
    {
        if (!g_logger) {
            DefaultInit();
        }
        g_logger->error(msg.c_str());
    }

    void Fatal(const std::string& msg)
    {
        if (!g_logger) {
            DefaultInit();
        }
        g_logger->critical(msg.c_str());
    }
}
