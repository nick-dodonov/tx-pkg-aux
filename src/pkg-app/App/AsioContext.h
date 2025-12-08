#pragma once
#include <boost/asio.hpp>
#include <boost/core/noncopyable.hpp>

namespace App
{
    class AsioContext : boost::noncopyable
    {
    public:
        AsioContext();
        ~AsioContext();

        [[nodiscard]] auto get_executor() { return _io_context.get_executor(); }

        void Run();
        int RunCoroMain(int argc, const char** argv, boost::asio::awaitable<int> coroMain);

    private:
        boost::asio::io_context _io_context;
        int _exitCode{};
    };
}
