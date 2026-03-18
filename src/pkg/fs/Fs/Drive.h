#pragma once
#include "Coro/Task.h"
#include "Path.h"

#include <boost/capy/io/any_read_source.hpp>
#include <boost/capy/read.hpp>
#include <boost/capy/buffers.hpp>
#include <boost/capy/buffers/string_dynamic_buffer.hpp>
#include <boost/capy/buffers/vector_dynamic_buffer.hpp>

#include <expected>
#include <string>
#include <system_error>
#include <vector>

namespace Fs
{
    namespace Detail
    {
        /// Overload set to create a DynamicBuffer adapter for a given container.
        inline auto MakeDynamicBuffer(std::string& s)
        {
            return boost::capy::string_dynamic_buffer(&s);
        }

        template<typename T, typename A>
            requires (std::is_fundamental_v<T> && sizeof(T) == 1)
        auto MakeDynamicBuffer(std::vector<T, A>& v)
        {
            return boost::capy::vector_dynamic_buffer(&v);
        }
    }

    class Drive
    {
    public:
        virtual ~Drive() = default;
        
        using PathResult = std::expected<Path, std::error_code>;
        [[nodiscard]] virtual PathResult GetNativePath(const Path& path) = 0;

        using SizeResult = std::expected<size_t, std::error_code>;
        [[nodiscard]] virtual SizeResult GetSize(const Path& path) = 0;

        using ReadResult = std::expected<size_t, std::error_code>;
        [[nodiscard]] virtual ReadResult ReadAllTo(const Path& path, boost::capy::mutable_buffer buf) = 0;

        /// Open a file as a ReadSource stream for flexible reading.
        using OpenResult = std::expected<boost::capy::any_read_source, std::error_code>;
        [[nodiscard]] virtual Coro::Task<OpenResult> OpenAsync(Path path) = 0;

        /// Read entire file into a byte container (std::vector<uint8_t>, std::string, etc.).
        /// Uses OpenAsync + Capy's dynamic buffer read algorithm internally.
        template<typename Container = std::vector<uint8_t>>
        [[nodiscard]] Coro::Task<std::expected<Container, std::error_code>>
        ReadAllAsync(Path path)
        {
            auto openResult = co_await OpenAsync(std::move(path));
            if (!openResult) {
                co_return std::unexpected(openResult.error());
            }

            Container container;
            auto [ec, n] = co_await boost::capy::read(
                *openResult, Detail::MakeDynamicBuffer(container));
            if (ec) {
                co_return std::unexpected(ec);
            }
            co_return container;
        }
    };
}
