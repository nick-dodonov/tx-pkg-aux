#include "LoopContext.h"

namespace Coro
{
    LoopContext::LoopContext()
        : boost::capy::execution_context(this)
    {}

    LoopContext::~LoopContext() = default;

    std::size_t LoopContext::poll()
    {
        std::size_t count = 0;
        _owner = std::this_thread::get_id();

        std::coroutine_handle<> h;
        while (_queue.try_dequeue(h)) {
            h.resume();
            ++count;
        }

        return count;
    }

    void LoopContext::stop()
    {
        //TODO: flag for stopping while processing the queue
    }
}
