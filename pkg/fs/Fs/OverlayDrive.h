#pragma once
#include "Drive.h"
#include <vector>

namespace Fs
{
    class OverlayDrive: public Drive
    {
    public:
        OverlayDrive(std::vector<Drive*> drives);
        ~OverlayDrive() override = default;

        [[nodiscard]] PathResult GetNativePath(const Path& path) override;
        [[nodiscard]] SizeResult GetSize(const Path& path) override;
        [[nodiscard]] ReadResult ReadAllTo(const Path& path, std::vector<uint8_t>& buf) override;

    private:
        std::vector<Drive*> _drives;
    };
}
