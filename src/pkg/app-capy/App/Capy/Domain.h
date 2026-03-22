#include "RunLoop/Handler.h"
#include "Coro/Task.h"
#include "Coro/LoopContext.h"

//#include <boost/capy/ex/thread_pool.hpp>
#include <boost/core/noncopyable.hpp>
#include <memory>

namespace Coro
{
    class Domain
        : public RunLoop::Handler
        , public std::enable_shared_from_this<class Domain>
        , boost::noncopyable
    {
    public:
        Domain(Coro::Task<int> mainTask);
        ~Domain();

    private:
        Coro::Task<int> _mainTask;
        //boost::capy::thread_pool _pool{1};  //TODO: replace w/ loop impl
        Coro::LoopContext _context;

        // Loop::Handler
        bool Start() override;
        void Stop() override;
        void Update(const RunLoop::UpdateCtx& ctx) override;
    };
}
