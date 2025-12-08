#pragma once
#include <boost/asio.hpp>
#include <boost/core/noncopyable.hpp>

namespace App
{
    class AsioContext : boost::noncopyable
    {
    public:
        AsioContext(int argc, const char** argv);
        ~AsioContext();

        [[nodiscard]] auto GetExecutor() { return _io_context.get_executor(); }
        void RunUntilStopped();

        int RunCoroMain(boost::asio::awaitable<int> coroMain);

    private:
        int _argc;
        const char** _argv;

        boost::asio::io_context _io_context;
        int _exitCode{};
    };
}
