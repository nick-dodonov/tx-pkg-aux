#include "OverlayDrive.h"

namespace Fs
{
    OverlayDrive::OverlayDrive(std::vector<Drive*> drives)
        : _drives(std::move(drives))
    {}

    Drive::PathResult OverlayDrive::GetNativePath(const Path& path)
    {
        std::error_code lastError = std::make_error_code(std::errc::no_such_file_or_directory);

        for (auto* drive : _drives) {
            if (!drive) {
                continue;
            }

            auto result = drive->GetNativePath(path);
            if (result.has_value()) {
                return result;
            }

            lastError = result.error();
        }

        return std::unexpected(lastError);
    }

    Coro::Task<Drive::OpenResult> OverlayDrive::OpenAsync(Path path)
    {
        std::error_code lastError = std::make_error_code(std::errc::no_such_file_or_directory);

        for (auto* drive : _drives) {
            if (!drive) {
                continue;
            }

            auto result = co_await drive->OpenAsync(path);
            if (result.has_value()) {
                co_return std::move(result);
            }

            lastError = result.error();
        }

        co_return std::unexpected(lastError);
    }
}
