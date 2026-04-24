#include "Log.h"
#include "Details/Format.h"
#include "StartupSink.h"

#include <spdlog/sinks/ansicolor_sink.h>

namespace Log
{
    Logger Logger::Default;
    std::shared_ptr<AreaSupplier> Logger::DummyAreaSupplier = std::make_shared<ConstAreaSupplier>(nullptr);

    void RegisterThread(std::string name)
    {
        Detail::RegisterThread(std::move(name));
    }

    std::string GetThreadName()
    {
        return Detail::GetCurrentThreadName();
    }

    size_t GetThreadId() noexcept
    {
        return Detail::GetCurrentThreadId();
    }
}

namespace Log::Detail
{
    static constexpr auto DefaultSpdlogLevel = spdlog::level::trace;

    spdlog::logger* InitLogger()
    {
        spdlog::set_level(DefaultSpdlogLevel);
        spdlog::set_formatter(MakeDefaultFormatter());

        auto* logger = spdlog::default_logger_raw();

        // TODO: make default sinks myself
#ifdef __cpp_rtti
        // Fix ansicolor_sink trace level color (default is too light, make it gray)
        for (auto& sink : logger->sinks()) {
            if (auto* ansicolor_mt = dynamic_cast<spdlog::sinks::ansicolor_stdout_sink_mt*>(sink.get())) {
                ansicolor_mt->set_color(spdlog::level::trace, ansicolor_mt->dark);
            } else if (auto* ansicolor_st = dynamic_cast<spdlog::sinks::ansicolor_stdout_sink_st*>(sink.get())) {
                ansicolor_st->set_color(spdlog::level::trace, ansicolor_st->dark);
            }
        }
#endif

        // Add StartupSink to buffer early logs before main sinks are ready
        if (auto startupSink = GetStartupSink()) {
            logger->sinks().emplace_back(std::move(startupSink));
        }

        return logger;
    }
}
