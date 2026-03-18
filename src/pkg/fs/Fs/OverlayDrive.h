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
        [[nodiscard]] ReadResult ReadAllTo(const Path& path, boost::capy::mutable_buffer buf) override;
        [[nodiscard]] Coro::Task<OpenResult> OpenAsync(Path path) override;

    private:
        std::vector<Drive*> _drives;
    };
}
