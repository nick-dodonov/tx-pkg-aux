#pragma once
#include "RunLoop/Handler.h"
#include <boost/asio.hpp>
#include <boost/core/noncopyable.hpp>
#include <memory>

namespace Asio
{
    class AsioDomain
        : public RunLoop::Handler
        , public std::enable_shared_from_this<AsioDomain>
        , boost::noncopyable
    {
    public:
        explicit AsioDomain(boost::asio::awaitable<int> coroMain);
        ~AsioDomain() override;

        /// Retrieve AsioDomain from any executor tied to its io_context.
        /// Safe to call from strands — ex.context() always yields the base io_context.
        [[nodiscard]] static AsioDomain* FromExecutor(const boost::asio::any_io_executor& ex)
        {
            auto& ctx = ex.context();
            if (boost::asio::has_service<Service>(ctx)) {
                return boost::asio::use_service<Service>(ctx).GetDomain();
            }
            return nullptr;
        }

    private:
        /// Boost.Asio execution_context service that allows retrieving AsioDomain
        /// from any executor associated with its io_context (including strands).
        ///
        /// Usage inside a coroutine running on AsioDomain's executor:
        ///   auto& domain = AsioDomain::FromExecutor(co_await asio::this_coro::executor);
        class Service: public boost::asio::execution_context::service
        {
        public:
            using key_type = Service;

            /// Default-constructible: required by use_service<> when the service
            /// has not been registered yet (i.e. before AsioDomain is constructed).
            /// _domain is set to this by AsioDomain::AsioDomain().
            explicit Service(boost::asio::execution_context& ctx)
                : boost::asio::execution_context::service(ctx)
            {}

            explicit Service(boost::asio::execution_context& ctx, AsioDomain* _domain)
                : boost::asio::execution_context::service(ctx)
                , _domain(_domain)
            {}

            /// Returns the AsioDomain associated with this io_context, or nullptr
            /// if AsioDomain has not been constructed yet.
            [[nodiscard]] AsioDomain* GetDomain() noexcept { return _domain; }

        private:
            void shutdown() override {}

            AsioDomain* _domain{};
            friend class AsioDomain;
        };

        boost::asio::io_context _io_context;
        boost::asio::awaitable<int> _coroMain;
        boost::asio::cancellation_signal _cancelSignal;
        bool _cancelled = false; ///< Set by Stop() before emitting the cancellation signal.

        // Loop::Handler
        bool Start() override;
        void Stop() override;
        void Update(const RunLoop::UpdateCtx& ctx) override;

        void Completed(const std::exception_ptr& ex, int exitCode);

        friend class AsioDomainService;
    };

    [[nodiscard]] boost::asio::awaitable<boost::system::error_code> AsyncCancelled();
}
