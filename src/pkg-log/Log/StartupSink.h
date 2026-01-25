#pragma once
#include <deque>
#include <mutex>
#include <spdlog/details/log_msg_buffer.h>
#include <spdlog/sinks/base_sink.h>

namespace Log::Detail
{
    /// Buffers log messages during early startup before main sinks are ready.
    /// Replays buffered messages when new sinks are added.
    template <typename Mutex>
    class StartupSink final: public spdlog::sinks::base_sink<Mutex>
    {
    public:
        explicit StartupSink(size_t maxBufferSize = 1000)
            : _maxBufferSize(maxBufferSize)
        {}

        /// Replay all buffered messages to the target sink
        void ReplayTo(spdlog::sinks::sink& targetSink)
        {
            std::lock_guard<Mutex> lock(this->mutex_);
            for (const auto& bufferedMsg : _buffer) {
                targetSink.log(bufferedMsg);
            }
        }

        /// Clear all buffered messages (called after replay)
        void ClearBuffer()
        {
            std::lock_guard<Mutex> lock(this->mutex_);
            _buffer.clear();
        }

    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override
        {
            // Store a copy of the message in circular buffer
            if (_buffer.size() >= _maxBufferSize) {
                _buffer.pop_front();
            }
            _buffer.emplace_back(msg);
        }

        void flush_() override
        {
            // Nothing to flush for buffered sink
        }

    private:
        std::deque<spdlog::details::log_msg_buffer> _buffer;
        size_t _maxBufferSize;
    };

    using StartupSinkMt = StartupSink<std::mutex>;
    using StartupSinkSt = StartupSink<spdlog::details::null_mutex>;

    /// Get global startup sink instance
    std::shared_ptr<StartupSinkMt> GetStartupSink();
}
