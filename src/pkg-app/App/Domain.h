#pragma once
#include "Boot/CliArgs.h"
#include <boost/asio.hpp>
#include <boost/core/noncopyable.hpp>

namespace App
{
    class Domain
        : public std::enable_shared_from_this<Domain>
        , boost::noncopyable
    {
    public:
        Domain(int argc, const char** argv);
        explicit Domain(Boot::CliArgs cliArgs);
        ~Domain();

        const auto& GetCliArgs() const { return _cliArgs; }

        [[nodiscard]] auto GetExecutor() { return _io_context.get_executor(); }
        void RunUntilStopped();

        int RunCoroMain(boost::asio::awaitable<int> coroMain);

    private:
        Boot::CliArgs _cliArgs;
        boost::asio::io_context _io_context;
        int _exitCode{};
    };
}
