#include "simple_log.h"

namespace simple_log {
    void info(const std::string& message) {
        std::cout << "[INFO] " << message << std::endl;
    }
    
    void error(const std::string& message) {
        std::cerr << "[ERROR] " << message << std::endl;
    }
}