#include <boost/throw_exception.hpp>
#include "Log/Log.h"

#ifdef BOOST_NO_EXCEPTIONS
namespace boost {
    BOOST_NORETURN void throw_exception(std::exception const& e)
    {
        Log::FatalF("boost::throw_exception - {}", e.what());
        std::abort();
    }
    void throw_exception(std::exception const& e, boost::source_location const& loc)
    {
        Log::FatalF("boost::throw_exception - {} at {}:{}:{}", e.what(), loc.file_name(), loc.line(), loc.column());
        std::abort();
    }
}
#endif
