#include "Log.h"
#include "Sink.h"
#include "StartupSink.h"

namespace Log::Detail
{
    void AddSink(std::shared_ptr<Sink> sink)
    {
        if (auto* logger = Detail::DefaultLoggerRaw()) {
            // Replay buffered startup messages to the new sink
            if (auto startupSink = GetStartupSink()) {
                startupSink->ReplayTo(*sink);
                // Clear buffer after first replay to free memory
                startupSink->ClearBuffer();
            }

            logger->sinks().emplace_back(std::move(sink));
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
