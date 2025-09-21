#include "Log.h"
#include <lwlog.h>

namespace Log
{
    static std::shared_ptr<lwlog::basic_logger<lwlog::sinks::stdout_sink>> g_logger;

    void DefaultInit()
    {
        g_logger = std::make_shared<
            lwlog::basic_logger<lwlog::sinks::stdout_sink>
        >("GLOBAL");

        //g_logger->set_level_filter(lwlog::level::info | lwlog::level::debug | lwlog::level::critical);

        //TODO: check/fix in web-tools and emrun
        //g_logger->set_pattern("[%T] [%n] [%l]: %v");
        //g_logger->set_pattern("{file} .red([%T] [%n]) .dark_green([:^12{level}]): .cyan(%v) TEXT");

        // https://github.com/ChristianPanov/lwlog/tree/v1.4.0?tab=readme-ov-file#syntax
#if defined(__EMSCRIPTEN__)
#define SUBSECS ":>3%e" // wasm precision isn't good
#else
#define SUBSECS ":>6%f" // microseconds is enough for native
#endif
        //TODO: {topic} supported
        //TODO: fix bug adding '.' in time crashes
        g_logger->set_pattern("[{time}·" SUBSECS "] :<8%l [%n] %v");

        g_logger->info("Log.DefaultInit");
    }

    void debug(const std::string& message) {
        if (!g_logger) DefaultInit();
        g_logger->debug(message.c_str());
    }

    void info(const std::string& message) {
        if (!g_logger) DefaultInit();
        g_logger->info(message.c_str());
    }

    void warn(const std::string& message) {
        if (!g_logger) DefaultInit();
        g_logger->warning(message.c_str());
    }

    void error(const std::string& message) {
        if (!g_logger) DefaultInit();
        g_logger->error(message.c_str());
    }

    void fatal(const std::string& message) {
        if (!g_logger) DefaultInit();
        g_logger->critical(message.c_str());
    }
}
