#pragma once

#include <filesystem>
#include <string_view>

namespace Fs
{
    /// Wrapper over std::filesystem::path providing zero-allocation view decomposition methods.
    /// All view methods return string_view into the internal native() storage.
    class Path : public std::filesystem::path
    {
    public:
        using std::filesystem::path::path;

        Path(const std::filesystem::path& p) : std::filesystem::path(p) {}              // NOLINT(google-explicit-constructor)
        Path(std::filesystem::path&& p) noexcept : std::filesystem::path(std::move(p)) {} // NOLINT(google-explicit-constructor)

        Path& operator=(const std::filesystem::path& p) { std::filesystem::path::operator=(p); return *this; }
        Path& operator=(std::filesystem::path&& p) noexcept { std::filesystem::path::operator=(std::move(p)); return *this; }

        [[nodiscard]] std::string_view filename_view() const noexcept;
        [[nodiscard]] std::string_view stem_view() const noexcept;
        [[nodiscard]] std::string_view extension_view() const noexcept;
        [[nodiscard]] std::string_view parent_path_view() const noexcept;
    };
}
