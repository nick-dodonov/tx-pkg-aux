#pragma once
#include <memory>

namespace spdlog
{
    class formatter;
}

namespace Log::Detail
{
    std::unique_ptr<spdlog::formatter> MakeDefaultFormatter();
}
