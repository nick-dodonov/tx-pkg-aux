#pragma once
#include <string_view>

namespace Log::Path
{
    [[nodiscard]] std::string_view GetBasename(std::string_view path);
    [[nodiscard]] std::string_view GetWithoutExtension(std::string_view path);
}
