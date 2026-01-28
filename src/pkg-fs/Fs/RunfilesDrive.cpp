#include "RunfilesDrive.h"
#include "Boot/Boot.h"
#include "rules_cc/cc/runfiles/runfiles.h"

using ::rules_cc::cc::runfiles::Runfiles;

namespace Fs
{
    class RunfilesDrive::Impl
    {
    public:
        std::string workspaceName;
        std::unique_ptr<Runfiles> runfiles;

        Impl(std::string_view name)
            : workspaceName(name)
        {
            std::string error;
            const auto& args = Boot::GetDefaultArgs();
            const auto& argv0 = !args.empty() ? std::string(args[0]) : std::string();
            runfiles.reset(Runfiles::Create(argv0, &error));
            if (!runfiles) {
                // Exception handling is disabled, so we just leave runfiles as nullptr

                // TODO: WASM can support runfiles, research how to implement it
            }
        }
    };

    RunfilesDrive::RunfilesDrive(std::string_view workspaceName)
        : _impl(std::make_unique<Impl>(workspaceName))
    {}

    RunfilesDrive::~RunfilesDrive() = default;

    bool RunfilesDrive::IsSupported() const
    {
        return _impl && _impl->runfiles != nullptr;
    }

    Drive::PathResult RunfilesDrive::GetNativePath(std::string_view path)
    {
        if (!_impl || !_impl->runfiles) {
            return std::unexpected(std::make_error_code(std::errc::not_supported));
        }

        std::string runfilePath = _impl->workspaceName + "/" + std::string(path);
        std::string nativePath = _impl->runfiles->Rlocation(runfilePath);

        if (nativePath.empty()) {
            return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
        }

        return nativePath;
    }
}
