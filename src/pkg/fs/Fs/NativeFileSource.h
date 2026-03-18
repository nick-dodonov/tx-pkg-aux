#pragma once
#include <boost/capy/io_task.hpp>
#include <boost/capy/buffers.hpp>
#include <boost/capy/error.hpp>

#include <filesystem>
#include <fstream>
#include <span>

namespace Fs
{
    /// ReadSource wrapping std::ifstream for reading native filesystem files.
    /// Satisfies boost::capy::ReadSource concept for use with any_read_source.
    class NativeFileSource
    {
    public:
        explicit NativeFileSource(std::filesystem::path path);

        NativeFileSource(NativeFileSource&& other) noexcept;
        NativeFileSource& operator=(NativeFileSource&& other) noexcept;

        NativeFileSource(const NativeFileSource&) = delete;
        NativeFileSource& operator=(const NativeFileSource&) = delete;

        /// Partial read: reads up to buffer_size(buffers) bytes, returns immediately with available data.
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
        std::ifstream _file;
        bool _eof = false;
    };
}
