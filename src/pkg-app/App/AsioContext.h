#pragma once
#include <boost/asio.hpp>
#include <boost/core/noncopyable.hpp>

namespace App {
    class AsioContext : boost::noncopyable {
    public:
        AsioContext();
        ~AsioContext();

        AsioContext(const AsioContext&) = delete;
        AsioContext& operator=(const AsioContext&) = delete;
        AsioContext(AsioContext&&) = delete;
        AsioContext& operator=(AsioContext&&) = delete;

        [[nodiscard]] auto get_executor() { return _io_context.get_executor(); }

        [[nodiscard]] int Run();

    private:
        boost::asio::io_context _io_context;
    };
}
