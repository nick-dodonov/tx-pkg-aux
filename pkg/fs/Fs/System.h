#pragma once

namespace Fs
{
    class Drive;

    class System
    {
    public:
        static Drive& GetDefaultDrive();
    };
}
