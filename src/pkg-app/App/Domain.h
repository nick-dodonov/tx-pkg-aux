#pragma once
#include "Boot/CliArgs.h"
#include "Loop/IRunner.h"
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
        explicit Domain(Boot::CliArgs cliArgs);
        ~Domain();

        [[nodiscard]] const auto& GetCliArgs() const { return _cliArgs; }
        [[nodiscard]] auto GetExecutor() { return _io_context.get_executor(); }

        int RunCoroMain(const std::shared_ptr<Loop::IRunner>& runner, boost::asio::awaitable<int> coroMain);

    private:
        Boot::CliArgs _cliArgs;

        boost::asio::io_context _io_context;
        int _exitCode{};

        bool Started(Loop::IRunner& runner) override;
        void Stopping(Loop::IRunner& runner) override;
        bool Update(Loop::IRunner& runner, const Loop::UpdateCtx& ctx) override;
    };
}
