namespace Exec
{
    /// Minimal task node stored in the concurrent queue.
    ///
    /// Public so external operation states (e.g., DelaySharedState) can inherit
    /// from it and be enqueued via Enqueue(). Uses a raw function pointer instead
    /// of virtual dispatch — a pattern also used in stdexec::run_loop's intrusive
    /// queue. The execute pointer is set by the inheriting type's constructor to
    /// capture its concrete type without requiring a virtual base class.
    ///
    /// Lifetime: the inheriting node must not be destroyed while it is still in the queue.
    struct OperationBase
    {
        void (*execute)(OperationBase*) noexcept {};
    };
}
