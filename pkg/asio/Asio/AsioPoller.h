#pragma once
#include "RunLoop/Handler.h"
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/core/noncopyable.hpp>

namespace Asio
{
    /// A RunLoop::Handler that owns a boost::asio::io_context and polls it each frame.
    ///
    /// Use standalone when you need Asio I/O driven by the run loop without a top-level
    /// coroutine (e.g., for offloading single async operations from an exec::task).
    /// AsioDomain inherits from this class and adds a coroutine lifecycle on top.
    class AsioPoller
        : public RunLoop::Handler
        , boost::noncopyable
    {
    public:
        AsioPoller();
        ~AsioPoller() override;

        /// Returns the io_context owned by this poller.
        [[nodiscard]] boost::asio::io_context& GetIoContext() noexcept { return _io; }

        /// Returns the executor associated with the owned io_context.
        [[nodiscard]] boost::asio::io_context::executor_type GetExecutor() noexcept
        {
            return _io.get_executor();
        }

    protected:
        // RunLoop::Handler
        void Update(const RunLoop::UpdateCtx& ctx) override;

    private:
        boost::asio::io_context _io;
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> _workGuard;
    };
}
