#pragma once
#include "concurrentqueue.h"
#include <boost/capy/ex/execution_context.hpp>

#include <cstddef>
#include <thread>

namespace Coro
{
    /// A context for running coroutines on a single thread, similar to a GUI event loop.
    class LoopContext: public boost::capy::execution_context
    {
    public:
        class executor_type; // NOLINT(readability-identifier-naming)

        LoopContext();
        ~LoopContext();

        /// Drain the queue until empty
        std::size_t poll();

        /// Executor for this context (can be used to dispatch coroutines and post work items to this context).
        executor_type get_executor() noexcept;

        /// Signal the context to stop current processing.
        void stop();

    private:
        friend class executor_type;

        moodycamel::ConcurrentQueue<std::coroutine_handle<>> _queue;
        std::thread::id _owner;

        void enqueue(std::coroutine_handle<> h) { _queue.enqueue(h); }
        bool is_running_on_this_thread() const noexcept { return std::this_thread::get_id() == _owner; }
    };

    /// Executor for LoopContext. Satisfies the Executor concept.
    class LoopContext::executor_type
    {
        friend class LoopContext;
        explicit executor_type(LoopContext& loop) noexcept
            : _loop(&loop)
        {}

    public:
        executor_type() = default;

        [[nodiscard]] boost::capy::execution_context& context() const noexcept { return *_loop; }

        void on_work_started() const noexcept {}
        void on_work_finished() const noexcept {}

        [[nodiscard]] std::coroutine_handle<> dispatch(std::coroutine_handle<> h) const
        {
            if (_loop->is_running_on_this_thread()) {
                return h;
            }

            _loop->enqueue(h);
            return std::noop_coroutine();
        }

        void post(std::coroutine_handle<> h) const { _loop->enqueue(h); }

        bool operator==(executor_type const& other) const noexcept { return _loop == other._loop; }

    private:
        LoopContext* _loop = nullptr;
    };

    // Verify the concept is satisfied
    static_assert(boost::capy::Executor<LoopContext::executor_type>);

    inline LoopContext::executor_type LoopContext::get_executor() noexcept
    {
        return executor_type{*this};
    }
}
