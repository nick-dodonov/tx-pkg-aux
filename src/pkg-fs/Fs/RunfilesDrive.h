#pragma once
#include "Drive.h"
#include <memory>

namespace Fs
{
    class RunfilesDrive: public Drive
    {
    public:
        RunfilesDrive(std::string_view workspaceName);
        ~RunfilesDrive() override;

        [[nodiscard]] bool IsSupported() const;
        [[nodiscard]] PathResult GetNativePath(std::string_view path) override;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };
}
