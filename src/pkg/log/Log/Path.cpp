#include "Path.h"

namespace Log::Path
{
    std::string_view GetBasename(const std::string_view path)
    {
        const auto pos = path.find_last_of("/\\");
        if (pos == std::string_view::npos) {
            // ReSharper disable once CppDFALocalValueEscapesFunction
            return path;
        }
        return path.substr(pos + 1);
    }

    std::string_view GetWithoutExtension(const std::string_view path)
    {
        const auto pos = path.find_last_of('.');
        if (pos == std::string_view::npos) {
            // ReSharper disable once CppDFALocalValueEscapesFunction
            return path;
        }
        return path.substr(0, pos);
    }
}
