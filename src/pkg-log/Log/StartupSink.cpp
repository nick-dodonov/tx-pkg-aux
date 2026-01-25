#include "StartupSink.h"

namespace Log::Detail
{
    std::shared_ptr<StartupSinkMt> GetStartupSink()
    {
        static auto startupSink = std::make_shared<StartupSinkMt>();
        return startupSink;
    }
}
