#include "Log.h"
#include <spdlog/spdlog.h>

namespace Log
{
    static std::shared_ptr<spdlog::logger> _logger;

    void DefaultInit()
    {
        spdlog::set_level(spdlog::level::trace);

        // https://github.com/gabime/spdlog/wiki/Custom-formatting
        spdlog::set_pattern("(%T.%f) %t %^[%L]%$ [%n] {%!} %v");

        _logger = spdlog::default_logger();
    }

    void Debug(const std::string& msg)
    {
        if (!_logger) {
            DefaultInit();
        }
        _logger->debug(msg.c_str());
    }

    void Info(const std::string& msg)
    {
        if (!_logger) {
            DefaultInit();
        }
        _logger->info(msg.c_str());
    }

    void Warn(const std::string& msg)
    {
        if (!_logger) {
            DefaultInit();
        }
        _logger->warn(msg.c_str());
    }

    void Error(const std::string& msg)
    {
        if (!_logger) {
            DefaultInit();
        }
        _logger->error(msg.c_str());
    }

    void Fatal(const std::string& msg)
    {
        if (!_logger) {
            DefaultInit();
        }
        _logger->critical(msg.c_str());
    }
}
