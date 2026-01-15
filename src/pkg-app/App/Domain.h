#pragma once
#include "Async/Mutex.h"
#include "Loop/Handler.h"
#include <boost/asio.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/core/noncopyable.hpp>
#include <memory>

namespace App
{
    class Domain
        : public Loop::Handler
        , public std::enable_shared_from_this<class Domain>
        , boost::noncopyable
    {
    public:
        Domain();
        ~Domain();

        [[nodiscard]] auto GetExecutor() { return _io_context.get_executor(); }

        int RunCoroMain(const std::shared_ptr<Loop::IRunner>& runner, boost::asio::awaitable<int> coroMain);
        boost::asio::awaitable<boost::system::error_code> AsyncStopped();

    private:
        boost::asio::io_context _io_context;

        using StopChannel = boost::asio::experimental::channel<void(boost::system::error_code)>;
        Async::Mutex _mutex;
        std::shared_ptr<StopChannel> _stopChannel ASYNC_GUARDED_BY(_mutex);

        // Loop::Handler interface
        bool Start() override;
        void Stop() override;
        void Update(const Loop::UpdateCtx& ctx) override;
    };
}
