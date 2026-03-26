#pragma once
#include "Drive.h"
#include <memory>

namespace Fs
{
    class RunfilesDrive: public Drive
    {
    public:
        RunfilesDrive(std::string_view workspaceName, std::shared_ptr<Drive> nativeDrive = {});
        ~RunfilesDrive() override;

        [[nodiscard]] bool IsSupported() const;
        [[nodiscard]] PathResult GetNativePath(const Path& path) override;
        [[nodiscard]] SizeResult GetSize(const Path& path) override;
        [[nodiscard]] ReadResult ReadAllTo(const Path& path, std::vector<uint8_t>& buf) override;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
        std::shared_ptr<Drive> _nativeDrive;
    };
}
