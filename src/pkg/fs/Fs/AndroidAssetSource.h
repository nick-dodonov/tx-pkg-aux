#pragma once
#ifdef __ANDROID__

#include <boost/capy/io_task.hpp>
#include <boost/capy/buffers.hpp>
#include <boost/capy/error.hpp>

#include <span>

struct AAsset;

namespace Fs
{
    /// ReadSource wrapping Android AAsset for reading APK assets.
    /// Satisfies boost::capy::ReadSource concept for use with any_read_source.
    /// Takes ownership of the AAsset* and closes it on destruction.
    class AndroidAssetSource
    {
    public:
        explicit AndroidAssetSource(AAsset* asset);
        ~AndroidAssetSource();

        AndroidAssetSource(AndroidAssetSource&& other) noexcept;
        AndroidAssetSource& operator=(AndroidAssetSource&& other) noexcept;

        AndroidAssetSource(const AndroidAssetSource&) = delete;
        AndroidAssetSource& operator=(const AndroidAssetSource&) = delete;

        /// Partial read: reads up to buffer_size(buffers) bytes.
        boost::capy::io_task<std::size_t>
        read_some(std::span<boost::capy::mutable_buffer const> buffers);

        /// Overload for single buffer (required by ReadStream concept).
        boost::capy::io_task<std::size_t>
        read_some(boost::capy::mutable_buffer buf)
        {
            return read_some(std::span<boost::capy::mutable_buffer const>(&buf, 1));
        }

        /// Complete read: fills the entire buffer sequence or returns EOF/error.
        boost::capy::io_task<std::size_t>
        read(std::span<boost::capy::mutable_buffer const> buffers);

        /// Overload for single buffer (required by ReadStream concept).
        boost::capy::io_task<std::size_t>
        read(boost::capy::mutable_buffer buf)
        {
            return read(std::span<boost::capy::mutable_buffer const>(&buf, 1));
        }

    private:
        AAsset* _asset = nullptr;
        bool _eof = false;
    };
}

#endif // __ANDROID__
