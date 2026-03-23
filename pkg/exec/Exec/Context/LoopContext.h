#pragma once
#include <stdexec/execution.hpp>

#include <cstddef>

namespace Exec
{
    /// Concept for frame-oriented, single-threaded execution contexts.
    ///
    /// A LoopContext provides a P2300-compatible scheduler that enqueues work into
    /// an internal queue, plus a manual drain operation to flush all pending work.
    ///
    /// Satisfied by PureLoopContext and TimedLoopContext.
    template <class T>
    concept LoopContext = requires(T& ctx) {
        { ctx.GetScheduler() } -> stdexec::scheduler;
        { ctx.DrainQueue() } -> std::convertible_to<std::size_t>;
    };

}
