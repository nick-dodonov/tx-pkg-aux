#pragma once
#include <exec/create.hpp>
#include <stdexec/execution.hpp>

#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <variant>

namespace Demo
{
    /// Commands from the user input layer.
    struct CmdConnect    { std::string peerId; };
    struct CmdDisconnect { std::string peerId; };
    struct CmdShot       {};
    struct CmdStatus     {};
    struct CmdSetSpeed   { float speed; };
    struct CmdQuit       {};

    using Command = std::variant<CmdConnect, CmdDisconnect, CmdShot, CmdStatus, CmdSetSpeed, CmdQuit>;

    /// Abstract command source interface.
    /// Implementations deliver user commands into the exec coroutine world
    /// via AwaitCommand(), which returns a sender that completes with the next
    /// command or nullopt on EOF / source closed.
    class ICommandSource
    {
    public:
        virtual ~ICommandSource() = default;

        /// One-shot callback set by AwaitCommand sender.
        using Callback = std::function<void(std::optional<Command>)>;

        /// Request the next command. If one is already queued, invoke cb synchronously.
        /// Otherwise store cb and invoke it when a command arrives.
        virtual void Request(Callback cb) = 0;
    };

    /// Returns a sender that completes with the next Command (or nullopt on EOF).
    inline auto AwaitCommand(ICommandSource& source)
    {
        return exec::create<stdexec::set_value_t(std::optional<Command>)>(
            [&source](auto& ctx) noexcept {
                source.Request([&ctx](std::optional<Command> cmd) {
                    stdexec::set_value(std::move(ctx.receiver), std::move(cmd));
                });
            });
    }

    /// Stdin-based command source. Runs a reader thread that parses simple text commands.
    /// Thread-safe: the reader thread enqueues; the exec thread dequeues via Request().
    class StdinCommandSource : public ICommandSource
    {
    public:
        StdinCommandSource()
        {
            _thread = std::jthread([this](std::stop_token stop) { ReaderLoop(stop); });
        }

        ~StdinCommandSource() override
        {
            _thread.request_stop();
            // Close stdin to unblock getline — platform-specific; on POSIX we just let the thread die.
        }

        void Request(Callback cb) override
        {
            std::lock_guard lock(_mutex);
            if (!_queue.empty()) {
                auto cmd = std::move(_queue.front());
                _queue.pop();
                cb(std::move(cmd));
            } else {
                _waiter = std::move(cb);
            }
        }

    private:
        void ReaderLoop(std::stop_token stop)
        {
            std::string line;
            while (!stop.stop_requested() && std::getline(std::cin, line)) {
                auto cmd = ParseLine(line);
                if (!cmd) {
                    continue;
                }
                std::lock_guard lock(_mutex);
                if (_waiter) {
                    auto w = std::move(_waiter);
                    _waiter = nullptr;
                    w(std::move(cmd));
                } else {
                    _queue.push(std::move(cmd));
                }
            }
            // EOF — notify waiter.
            std::lock_guard lock(_mutex);
            if (_waiter) {
                auto w = std::move(_waiter);
                _waiter = nullptr;
                w(std::nullopt);
            }
        }

        static std::optional<Command> ParseLine(const std::string& line)
        {
            if (line.empty()) {
                return std::nullopt;
            }
            // Split on first space.
            auto sp = line.find(' ');
            auto verb = (sp == std::string::npos) ? line : line.substr(0, sp);
            auto arg  = (sp == std::string::npos) ? "" : line.substr(sp + 1);

            if (verb == "connect" && !arg.empty())    { return CmdConnect{arg}; }
            if (verb == "disconnect" && !arg.empty())  { return CmdDisconnect{arg}; }
            if (verb == "shot")                        { return CmdShot{}; }
            if (verb == "status")                      { return CmdStatus{}; }
            if (verb == "speed" && !arg.empty())       { return CmdSetSpeed{std::stof(arg)}; }
            if (verb == "quit" || verb == "exit")      { return CmdQuit{}; }
            return std::nullopt;
        }

        std::mutex _mutex;
        std::queue<std::optional<Command>> _queue;
        Callback _waiter;
        std::jthread _thread;
    };

    /// Null command source that never yields commands. Used for in-process scenario nodes.
    class NullCommandSource : public ICommandSource
    {
    public:
        void Request(Callback /*cb*/) override
        {
            // Never fires — scenario-driven nodes don't read commands.
        }
    };
}
