#pragma once
#include <iostream>
#include <string>

namespace simple_log {
    void info(const std::string& message);
    void error(const std::string& message);
}