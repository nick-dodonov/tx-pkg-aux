#pragma once
#include "Drive.h"
#include <memory>

namespace Fs
{
    class RunfilesDrive: public Drive
    {
    public:
        RunfilesDrive(std::string_view workspaceName, Drive* nativeDrive = nullptr);
        ~RunfilesDrive() override;

        [[nodiscard]] bool IsSupported() const;
        [[nodiscard]] PathResult GetNativePath(const Path& path) override;
        [[nodiscard]] SizeResult GetSize(const Path& path) override;
        [[nodiscard]] ReadResult ReadAllTo(const Path& path, boost::capy::mutable_buffer buf) override;
        [[nodiscard]] Coro::Task<OpenResult> OpenAsync(Path path) override;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
        Drive* _nativeDrive = nullptr;
    };
}
