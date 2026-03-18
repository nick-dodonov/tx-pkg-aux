#include "NativeFileSource.h"

namespace Fs
{
    NativeFileSource::NativeFileSource(std::filesystem::path path)
        : _file(path, std::ios::binary)
    {
    }

    NativeFileSource::NativeFileSource(NativeFileSource&& other) noexcept
        : _file(std::move(other._file))
        , _eof(other._eof)
    {
        other._eof = true;
    }

    NativeFileSource& NativeFileSource::operator=(NativeFileSource&& other) noexcept
    {
        if (this != &other) {
            _file = std::move(other._file);
            _eof = other._eof;
            other._eof = true;
        }
        return *this;
    }

    boost::capy::io_task<std::size_t>
    NativeFileSource::read_some(std::span<boost::capy::mutable_buffer const> buffers)
    {
        using namespace boost::capy;

        if (_eof || !_file.is_open()) {
            co_return {error::eof, 0};
        }

        std::size_t totalRead = 0;
        for (auto const& buf : buffers) {
            if (buf.size() == 0) {
                continue;
            }

            _file.read(static_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
            auto bytesRead = static_cast<std::size_t>(_file.gcount());
            totalRead += bytesRead;

            if (_file.eof()) {
                _eof = true;
                break;
            }

            if (_file.fail()) {
                co_return {std::make_error_code(std::errc::io_error), totalRead};
            }

            // read_some returns after first successful read (partial I/O)
            if (totalRead > 0) {
                break;
            }
        }

        if (totalRead == 0 && _eof) {
            co_return {error::eof, 0};
        }

        co_return {{}, totalRead};
    }

    boost::capy::io_task<std::size_t>
    NativeFileSource::read(std::span<boost::capy::mutable_buffer const> buffers)
    {
        using namespace boost::capy;

        if (_eof || !_file.is_open()) {
            co_return {error::eof, 0};
        }

        std::size_t totalRead = 0;
        for (auto const& buf : buffers) {
            std::size_t remaining = buf.size();
            auto* ptr = static_cast<char*>(buf.data());

            while (remaining > 0) {
                _file.read(ptr, static_cast<std::streamsize>(remaining));
                auto bytesRead = static_cast<std::size_t>(_file.gcount());
                totalRead += bytesRead;
                ptr += bytesRead;
                remaining -= bytesRead;

                if (_file.eof()) {
                    _eof = true;
                    co_return {error::eof, totalRead};
                }

                if (_file.fail()) {
                    co_return {std::make_error_code(std::errc::io_error), totalRead};
                }
            }
        }

        co_return {{}, totalRead};
    }
}
