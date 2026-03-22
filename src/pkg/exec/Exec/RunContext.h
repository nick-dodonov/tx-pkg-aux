#pragma once

#include "Exec/TimedLoopContext.h"

namespace Exec
{
    /// Project-level alias for the execution context used by App::Exec::Domain.
    ///
    /// Currently bound to TimedLoopContext (PureLoopContext + ITimerBackend). The
    /// alias provides a stable name for code that depends on the domain's context
    /// type without coupling to the underlying implementation class.
    ///
    /// RunContext::Scheduler satisfies both stdexec::scheduler and
    /// exec::timed_scheduler, enabling exec::schedule_after / exec::schedule_at.
    using RunContext = TimedLoopContext;
}
