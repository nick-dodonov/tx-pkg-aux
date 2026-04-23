#include "Log/Exception.h"

namespace Log
{
    std::string ExceptionMessage(std::exception_ptr ep)
    {
        if (!ep) {
            return {};
        }
        try {
            std::rethrow_exception(ep);
        }
        catch (const std::exception& e) {
            return e.what();
        }
        catch (...) {
            return "<unknown exception>";
        }
    }
}
