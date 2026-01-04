#pragma once
#include "Boot/CliArgs.h"
#include "Loop/ILooper.h"
#include <boost/asio.hpp>
#include <boost/core/noncopyable.hpp>
#include <memory>

namespace App
{
    class Domain
        : public Loop::IHandler
        , public std::enable_shared_from_this<class Domain>
        , boost::noncopyable
    {
    public:
        Domain(int argc, const char** argv);
        Domain(int argc, const char** argv, std::shared_ptr<Loop::ILooper> looper);
        explicit Domain(Boot::CliArgs cliArgs);
        Domain(Boot::CliArgs cliArgs, std::shared_ptr<Loop::ILooper> looper);
        ~Domain();

        /// Get the looper, optionally cast to a specific type
        template <typename T = Loop::ILooper>
        [[nodiscard]] std::shared_ptr<T> GetLooper() const { return std::dynamic_pointer_cast<T>(_looper); }

        [[nodiscard]] const auto& GetCliArgs() const { return _cliArgs; }
        [[nodiscard]] auto GetExecutor() { return _io_context.get_executor(); }

        int RunCoroMain(boost::asio::awaitable<int> coroMain);

    private:
        Boot::CliArgs _cliArgs;
        std::shared_ptr<Loop::ILooper> _looper;

        boost::asio::io_context _io_context;
        int _exitCode{};

        void RunContext();

        bool Started(Loop::ILooper& looper) override;
        bool Update(Loop::ILooper& looper, const Loop::UpdateCtx& ctx) override;
        void Stopping(Loop::ILooper& looper) override;
    };
}
