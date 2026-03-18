#pragma once
#include "Drive.h"
#include <vector>

namespace Fs
{
    class NativeDrive: public Drive
    {
    public:
        NativeDrive(std::vector<Path> prefixPaths);

        [[nodiscard]] PathResult GetNativePath(const Path& path) override;
        [[nodiscard]] Coro::Task<OpenResult> OpenAsync(Path path) override;

    private:
        std::vector<Path> _prefixPaths;
    };
}
