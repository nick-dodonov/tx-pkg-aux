#include "Log.h"
#include <lwlog.h>

namespace Log
{
    // Глобальный logger для простоты использования
    static std::shared_ptr<lwlog::basic_logger<lwlog::sinks::stdout_sink>> g_logger;

    void DefaultInit()
    {
        // Простая демонстрация создания logger для разных версий lwlog
        // Версия 1.4.0: используем basic_logger (без registry)
        // Версия 1.3.1: используем basic_logger + registry
        
        g_logger = std::make_shared<
            lwlog::basic_logger<lwlog::sinks::stdout_sink>
        >("CONSOLE");
        
        // Пытаемся использовать registry, если он есть (версия 1.3.1)
        // Если нет - просто логируем (версия 1.4.0)
        #ifdef LWLOG_HAS_REGISTRY
            lwlog::registry::instance().register_logger(g_logger.get());
        #endif
        
        g_logger->info("Log.DefaultInit - lwlog version compatibility demonstrated!");
    }

    void info(const std::string& message) {
        if (!g_logger) DefaultInit();
        g_logger->info(message.c_str());
    }

    void warn(const std::string& message) {
        if (!g_logger) DefaultInit();
        g_logger->warning(message.c_str());  // используем warning вместо warn
    }

    void error(const std::string& message) {
        if (!g_logger) DefaultInit();
        g_logger->error(message.c_str());
    }
}
