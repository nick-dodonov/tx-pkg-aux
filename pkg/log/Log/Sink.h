#pragma once
#include <memory>
#include <spdlog/sinks/base_sink.h>

namespace Log::Detail
{
    using Sink = spdlog::sinks::sink;
    template <typename Mutex>
    using BaseSink = spdlog::sinks::base_sink<Mutex>;

    void AddSink(std::shared_ptr<Sink> sink);
    void RemoveSink(const std::shared_ptr<Sink>& sink);
}
