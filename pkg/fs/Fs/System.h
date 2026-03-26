#pragma once
#include "Fs/Drive.h"
#include <memory>

namespace Fs
{
    class System
    {
    public:
        static std::shared_ptr<Drive> MakeDefaultDrive();

        static Drive& GetDefaultDrive();
    };
}
