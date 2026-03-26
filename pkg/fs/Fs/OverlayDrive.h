#pragma once
#include "Drive.h"
#include <memory>
#include <vector>

namespace Fs
{
    class OverlayDrive: public Drive
    {
    public:
        template <typename... DrivePtrs>
        OverlayDrive(DrivePtrs&&... drives)
            : _drives{std::forward<DrivePtrs>(drives)...}
        {}
        ~OverlayDrive() override = default;

        [[nodiscard]] PathResult GetNativePath(const Path& path) override;
        [[nodiscard]] SizeResult GetSize(const Path& path) override;
        [[nodiscard]] ReadResult ReadAllTo(const Path& path, std::vector<uint8_t>& buf) override;

    private:
        std::vector<std::shared_ptr<Drive>> _drives;
    };
}
