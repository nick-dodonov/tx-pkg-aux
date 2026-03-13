#pragma once
#include "Drive.h"
#include <vector>

namespace Fs
{
    class NativeDrive: public Drive
    {
    public:
        NativeDrive(std::vector<std::string> prefixPaths);

        [[nodiscard]] PathResult GetNativePath(std::string_view path) override;

    private:
        std::vector<std::string> _prefixPaths;
    };
}
