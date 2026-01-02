#include "UpdateCtx.h"
#include "ILooper.h"

namespace App::Loop
{
    UpdateCtx::UpdateCtx(ILooper& looper)
        : Looper(looper)
    {
    }
}
