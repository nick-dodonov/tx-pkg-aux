#include "Log.h"
#include "Details/Format.h"
#include "StartupSink.h"

#include <spdlog/sinks/ansicolor_sink.h>

namespace Log
{
    Logger Logger::Default;
}

namespace Log::Detail
{
    static constexpr auto DefaultSpdlogLevel = spdlog::level::trace;

    spdlog::logger* InitLogger()
    {
        spdlog::set_level(DefaultSpdlogLevel);
        spdlog::set_formatter(MakeDefaultFormatter());

        auto* logger = spdlog::default_logger_raw();

        // Fix ansicolor_sink trace level color (default is too light, make it gray)
        for (auto& sink : logger->sinks()) {
            if (auto* ansicolor = dynamic_cast<spdlog::sinks::ansicolor_stdout_sink_mt*>(sink.get())) {
                ansicolor->set_color(spdlog::level::trace, ansicolor->dark);
            } else if (auto* ansicolor = dynamic_cast<spdlog::sinks::ansicolor_stdout_sink_st*>(sink.get())) {
                ansicolor->set_color(spdlog::level::trace, ansicolor->dark);
            }
        }

        // Add StartupSink to buffer early logs before main sinks are ready
        if (auto startupSink = GetStartupSink()) {
            logger->sinks().emplace_back(std::move(startupSink));
        }

        return logger;
    }
}
