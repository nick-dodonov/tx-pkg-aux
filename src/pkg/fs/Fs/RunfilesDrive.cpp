#include "RunfilesDrive.h"
#include "System.h"
#include "Boot/Boot.h"
#include "Log/Log.h"
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

    RunfilesDrive::RunfilesDrive(std::string_view workspaceName, Drive* nativeDrive)
        : _impl(std::make_unique<Impl>(workspaceName))
        , _nativeDrive(nativeDrive)
    {
        Log::Trace("{}", workspaceName);
        if (_nativeDrive == nullptr) {
            _nativeDrive = &Fs::System::GetDefaultDrive();
        }
    }

    RunfilesDrive::~RunfilesDrive() = default;

    bool RunfilesDrive::IsSupported() const
    {
        return _impl && _impl->runfiles != nullptr;
    }

    Drive::PathResult RunfilesDrive::GetNativePath(const Path& path)
    {
        if (!_impl || !_impl->runfiles) {
            return std::unexpected(std::make_error_code(std::errc::not_supported));
        }

        std::string runfilePath = _impl->workspaceName + "/" + path.generic_string();
        std::string nativePath = _impl->runfiles->Rlocation(runfilePath);

        if (nativePath.empty()) {
            return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
        }

        if (_nativeDrive) {
            // Verify file exists through native drive for better error handling
            return _nativeDrive->GetNativePath(Path(nativePath));
        }

        return Path(nativePath);
    }

    Coro::Task<Drive::ReadAllBytesResult> RunfilesDrive::ReadAllBytesAsync(Path path)
    {
        auto nativePathResult = GetNativePath(path);
        if (!nativePathResult) {
            co_return std::unexpected(nativePathResult.error());
        }

        if (_nativeDrive) {
            co_return co_await _nativeDrive->ReadAllBytesAsync(nativePathResult.value());
        }

        co_return std::unexpected(std::make_error_code(std::errc::not_supported));
    }
}
