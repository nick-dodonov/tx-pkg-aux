#include "Sink.h"
#include "Log.h"
#include <spdlog/sinks/sink.h>

namespace Log::Detail
{
    void AddSink(std::shared_ptr<Sink> sink)
    {
        if (auto* logger = Detail::DefaultLoggerRaw()) {
            logger->sinks().push_back(std::move(sink));
        }
    }

    void RemoveSink(const std::shared_ptr<Sink>& sink)
    {
        if (auto* logger = Detail::DefaultLoggerRaw()) {
            auto& sinks = logger->sinks();
            std::erase(sinks, sink);
        }
    }
}
