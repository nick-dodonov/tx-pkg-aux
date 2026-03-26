#pragma once
#include "Drive.h"
#include <memory>
#include <vector>

namespace Fs
{
    class NativeDrive: public Drive
    {
    public:
        template <typename... Paths>
        explicit NativeDrive(Paths&&... prefixPaths)
            : _prefixPaths{std::forward<Paths>(prefixPaths)...}
        {
            InitTrace();
        }
        template <typename... Paths>
        static std::shared_ptr<NativeDrive> Make(Paths&&... prefixPaths)
        {
            return std::make_shared<NativeDrive>(std::forward<Paths>(prefixPaths)...);
        }

        [[nodiscard]] PathResult GetNativePath(const Path& path) override;
        [[nodiscard]] SizeResult GetSize(const Path& path) override;
        [[nodiscard]] ReadResult ReadAllTo(const Path& path, std::vector<uint8_t>& buf) override;

    private:
        void InitTrace() const;
        std::vector<Path> _prefixPaths;
    };
}
