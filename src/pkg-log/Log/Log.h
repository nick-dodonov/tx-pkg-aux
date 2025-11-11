#pragma once
#include <format>
#include <string>

namespace Log {
    void DefaultInit();

    void Debug(const std::string& msg);
    void Info(const std::string& msg);
    void Warn(const std::string& msg);
    void Error(const std::string& msg);
    void Fatal(const std::string& msg);

    template <typename... Args>
    void DebugF(std::format_string<Args...> fmt, Args&&... args)
    {
        Debug(std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void InfoF(std::format_string<Args...> fmt, Args&&... args)
    {
        Info(std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void WarnF(std::format_string<Args...> fmt, Args&&... args)
    {
        Warn(std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void ErrorF(std::format_string<Args...> fmt, Args&&... args)
    {
        Error(std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void FatalF(std::format_string<Args...> fmt, Args&&... args)
    {
        Fatal(std::format(fmt, std::forward<Args>(args)...));
    }
}
