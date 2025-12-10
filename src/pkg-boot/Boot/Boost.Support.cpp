#include "Log/Log.h"
#include <boost/throw_exception.hpp>

#ifdef BOOST_NO_EXCEPTIONS
namespace boost
{
    BOOST_NORETURN void throw_exception(std::exception const& e)
    {
        Log::Fatal("boost::throw_exception - {}", e.what());
        std::abort();
    }
    BOOST_NORETURN void throw_exception(std::exception const& e, boost::source_location const& loc)
    {
        Log::Fatal("boost::throw_exception - {} at {}:{}:{}", e.what(), loc.file_name(), loc.line(), loc.column());
        std::abort();
    }
}
#endif
