#include "Log.h"

namespace Log::Details
{
    void DefaultInit()
    {
        spdlog::set_level(spdlog::level::trace);

        // https://github.com/gabime/spdlog/wiki/Custom-formatting
        spdlog::set_pattern("(%T.%f) %t %^[%L]%$ [%n] {%!} %v");
    }
}
