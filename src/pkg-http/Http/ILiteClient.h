#pragma once
#include <expected>
#include <string>
#include <functional>
#include <system_error>

namespace Http
{
    class ILiteClient
    {
    public:
        virtual ~ILiteClient() = default;

        using Result = std::expected<std::string, std::error_code>;
        using Callback = std::function<void(Result result)>;

        virtual void Get(std::string_view url, Callback&& handler) = 0;
    };
}
