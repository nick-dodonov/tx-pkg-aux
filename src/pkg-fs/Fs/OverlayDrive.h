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

        [[nodiscard]] PathResult GetNativePath(std::string_view path) override;

    private:
        std::vector<Drive*> _drives;
    };
}
