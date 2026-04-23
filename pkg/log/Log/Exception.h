#pragma once
#include <exception>
#include <string>

namespace Log
{
    // Returns the message from an exception_ptr.
    // Requires linking against //pkg/log:log-ex (compiled with -fexceptions).
    //
    // Returns:
    //   - ""                    if ep is nullptr
    //   - e.what()              if ep holds a std::exception
    //   - "<unknown exception>" otherwise
    std::string ExceptionMessage(std::exception_ptr ep);
}
