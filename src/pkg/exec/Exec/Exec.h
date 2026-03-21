#pragma once

#include <exec/task.hpp>
#include <stdexec/execution.hpp>

namespace Exec
{
    /// Re-export commonly used execution facilities for convenience of replacement 
    /// to std::execution in future C++26 adoption (libstdc++, libc++, msvc stl)
    using stdexec::schedule;
    using stdexec::scheduler_t;
    using stdexec::sender_t;
    using stdexec::completion_signatures;
    using stdexec::then;
    using stdexec::continues_on;
    using stdexec::get_stop_token;
    using stdexec::set_stopped;
    using stdexec::set_value;
    using stdexec::set_error;
    using stdexec::get_env;
    using stdexec::set_value_t;
    using stdexec::get_completion_scheduler_t;
    using stdexec::set_stopped_t;
}
