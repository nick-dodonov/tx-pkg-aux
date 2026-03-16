#include "Drive.h"

namespace Fs
{
    Coro::Task<Drive::ReadAllBytesResult> Drive::ReadAllBytesAsync(const Path& path)
    {
        co_return std::unexpected(std::make_error_code(std::errc::function_not_supported));
    }
}
