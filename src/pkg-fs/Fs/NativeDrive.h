#pragma once
#include "Drive.h"

namespace Fs
{
    class NativeDrive: public Drive
    {
    public:
        NativeDrive(std::string_view prefixPath);

        [[nodiscard]] PathResult GetNativePath(std::string_view path) override;

    private:
        std::string _prefixPath;
    };
}
