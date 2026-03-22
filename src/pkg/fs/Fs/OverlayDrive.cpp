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

    Drive::SizeResult OverlayDrive::GetSize(const Path& path)
    {
        std::error_code lastError = std::make_error_code(std::errc::no_such_file_or_directory);

        for (auto* drive : _drives) {
            if (!drive) {
                continue;
            }

            auto result = drive->GetSize(path);
            if (result.has_value()) {
                return result;
            }

            lastError = result.error();
        }

        return std::unexpected(lastError);
    }

    Drive::ReadResult OverlayDrive::ReadAllTo(const Path& path, std::vector<uint8_t>& buf)
    {
        std::error_code lastError = std::make_error_code(std::errc::no_such_file_or_directory);

        for (auto* drive : _drives) {
            if (!drive) {
                continue;
            }

            auto result = drive->ReadAllTo(path, buf);
            if (result.has_value()) {
                return result;
            }

            lastError = result.error();
        }

        return std::unexpected(lastError);
    }
}
