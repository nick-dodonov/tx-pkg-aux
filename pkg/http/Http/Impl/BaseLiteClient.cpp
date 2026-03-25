#include "BaseLiteClient.h"
#include "Log/Log.h"
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/co_spawn.hpp>

namespace Http
{
    BaseLiteClient::BaseLiteClient(boost::asio::any_io_executor executor)
        : _executor(std::move(executor))
    {}

    void BaseLiteClient::Get(std::string_view url, Callback&& handler, std::stop_token stopToken)
    {
        Log::Debug("http: query: {}", url);

        // Short-circuit if already stopped before spawning the coroutine
        if (stopToken.stop_requested()) {
            Log::Debug("http: query: cancelled before start: {}", url);
            handler(std::unexpected(std::system_error{
                std::make_error_code(std::errc::operation_canceled),
                "stop requested before request"
            }));
            return;
        }

        // Bridge std::stop_token → Asio cancellation_signal.
        // Single heap allocation owns both the signal and the stop_callback.
        struct StopBridge {
            boost::asio::cancellation_signal signal;
            using StopCb = std::stop_callback<std::function<void()>>;
            std::optional<StopCb> stopCb; // declared after signal — destroyed first

            explicit StopBridge(std::stop_token token) {
                if (token.stop_possible()) {
                    stopCb.emplace(std::move(token), [this]() {
                        signal.emit(boost::asio::cancellation_type::all);
                    });
                }
            }
        };

        auto stopBridge = std::make_unique<StopBridge>(std::move(stopToken));
        auto slot = stopBridge->signal.slot();

        boost::asio::co_spawn(
            _executor,
            GetAsync(std::string{url}),
            boost::asio::bind_cancellation_slot(
                slot,
                // Prevent signal & stopCb destruction until completion handler runs
                [handler = std::move(handler), stopBridge = std::move(stopBridge)]
                (const std::exception_ptr& e, Result&& result) {
                    if (e) {
                        // code never here with -fno-exceptions
                        Log::Error("http: query: exception");
                        handler(std::unexpected(std::system_error{
                            std::make_error_code(std::errc::state_not_recoverable),
                            "co_spawn: not handled exception"
                        }));
                    } else {
                        if (result) {
                            Log::Debug("http: query: succeed: {} size={}", result->statusCode, result->body.size());
                        } else {
                            Log::Error("http: query: failed: {}", result.error().what());
                        }
                        handler(std::move(result));
                    }
                }
            )
        );
    }
}
