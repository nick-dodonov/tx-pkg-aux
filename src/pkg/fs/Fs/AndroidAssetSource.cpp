#ifdef __ANDROID__
#include "AndroidAssetSource.h"

#include <android/asset_manager.h>
#include <algorithm>
#include <cstring>
#include <utility>

namespace Fs
{
    AndroidAssetSource::AndroidAssetSource(AAsset* asset)
        : _asset(asset)
    {
    }

    AndroidAssetSource::~AndroidAssetSource()
    {
        if (_asset) {
            AAsset_close(_asset);
        }
    }

    AndroidAssetSource::AndroidAssetSource(AndroidAssetSource&& other) noexcept
        : _asset(std::exchange(other._asset, nullptr))
        , _eof(other._eof)
    {
    }

    AndroidAssetSource& AndroidAssetSource::operator=(AndroidAssetSource&& other) noexcept
    {
        if (this != &other) {
            if (_asset) {
                AAsset_close(_asset);
            }
            _asset = std::exchange(other._asset, nullptr);
            _eof = other._eof;
        }
        return *this;
    }

    boost::capy::io_task<std::size_t>
    AndroidAssetSource::read_some(std::span<boost::capy::mutable_buffer const> buffers)
    {
        using namespace boost::capy;

        if (_eof || !_asset) {
            co_return {error::eof, 0};
        }

        std::size_t totalRead = 0;

        for (const auto& buf : buffers) {
            if (buf.size() == 0) {
                continue;
            }

            int bytesRead = AAsset_read(_asset, buf.data(), buf.size());
            if (bytesRead < 0) {
                co_return {std::make_error_code(std::errc::io_error), totalRead};
            }

            if (bytesRead == 0) {
                _eof = true;
                break;
            }

            totalRead += static_cast<std::size_t>(bytesRead);
            // read_some returns after first successful read
            break;
        }

        if (totalRead == 0 && _eof) {
            co_return {error::eof, 0};
        }

        co_return {{}, totalRead};
    }

    boost::capy::io_task<std::size_t>
    AndroidAssetSource::read(std::span<boost::capy::mutable_buffer const> buffers)
    {
        using namespace boost::capy;

        if (_eof || !_asset) {
            co_return {error::eof, 0};
        }

        std::size_t totalRead = 0;

        for (const auto& buf : buffers) {
            auto* dest = static_cast<char*>(buf.data());
            std::size_t remaining = buf.size();

            while (remaining > 0) {
                int bytesRead = AAsset_read(_asset, dest, remaining);
                if (bytesRead < 0) {
                    co_return {std::make_error_code(std::errc::io_error), totalRead};
                }

                if (bytesRead == 0) {
                    _eof = true;
                    if (totalRead == 0) {
                        co_return {error::eof, 0};
                    }
                    co_return {error::eof, totalRead};
                }

                totalRead += static_cast<std::size_t>(bytesRead);
                dest += bytesRead;
                remaining -= static_cast<std::size_t>(bytesRead);
            }
        }

        co_return {{}, totalRead};
    }
}

#endif // __ANDROID__
