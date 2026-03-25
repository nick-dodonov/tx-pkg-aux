#pragma once
#include <memory>
#include <expected>
#include <string>
#include <functional>
#include <stop_token>
#include <system_error>

namespace Http
{
    class ILiteClient : public std::enable_shared_from_this<ILiteClient>
    {
    public:
        virtual ~ILiteClient() = default;

        struct Response {
            int statusCode;
            std::string body;
        };
        using Result = std::expected<Response, std::system_error>;
        using Callback = std::function<void(Result result)>;

        virtual void Get(std::string_view url, Callback&& handler, std::stop_token stopToken = {}) = 0;
    };
}
