#pragma once
#include <string>

namespace Log {
    void DefaultInit();
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);
}
