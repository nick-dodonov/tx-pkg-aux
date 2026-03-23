#include "Path.h"

namespace Fs
{
    std::string_view Path::filename_view() const noexcept
    {
        std::string_view sv = native();
        if (sv.empty()) return {};

        // Strip trailing separators (unless the entire path is "/")
        auto end = sv.size();
        while (end > 1 && sv[end - 1] == '/') --end;

        auto pos = sv.rfind('/', end - 1);
        if (pos == std::string_view::npos) return sv.substr(0, end);
        return sv.substr(pos + 1, end - pos - 1);
    }

    std::string_view Path::stem_view() const noexcept
    {
        auto fname = filename_view();
        if (fname.empty() || fname == "." || fname == "..") return fname;

        auto dot = fname.rfind('.');
        // No dot, or dot is at position 0 (dotfile like ".bashrc") → entire filename is the stem
        if (dot == std::string_view::npos || dot == 0) return fname;
        return fname.substr(0, dot);
    }

    std::string_view Path::extension_view() const noexcept
    {
        auto fname = filename_view();
        if (fname.empty() || fname == "." || fname == "..") return {};

        auto dot = fname.rfind('.');
        if (dot == std::string_view::npos || dot == 0) return {};
        return fname.substr(dot);
    }

    std::string_view Path::parent_path_view() const noexcept
    {
        std::string_view sv = native();
        if (sv.empty()) return {};

        // Strip trailing separators (unless the entire path is "/")
        auto end = sv.size();
        while (end > 1 && sv[end - 1] == '/') --end;

        auto pos = sv.rfind('/', end - 1);
        if (pos == std::string_view::npos) return {};
        // Root "/" → return "/"
        if (pos == 0) return sv.substr(0, 1);
        return sv.substr(0, pos);
    }
}
