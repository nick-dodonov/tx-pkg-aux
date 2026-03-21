#include "App/Exec/Domain.h"
#include "App/Loop/Factory.h"
#include "Boot/Boot.h"
#include "Log/Log.h"
#include <exec/task.hpp>

/// Build a sender pipeline that runs on the update scheduler and returns an exit code
static auto MakeMainSender(Exec::UpdateScheduler::Scheduler sched)
{
    return
        stdexec::schedule(sched) 
        | stdexec::then([] { 
            Log::Info("[main] Task 1: Hello from update scheduler!"); 
        })
        | stdexec::continues_on(sched) 
        | stdexec::then([] {
            Log::Info("[main] Task 2: Computing answer...");
            return 42;
        });
}

static auto MainTask() -> exec::task<int>
{
    Log::Info("MainTask...");
    co_return 17;
}

int main(const int argc, const char* argv[])
{
    Boot::DefaultInit(argc, argv);

    auto domain = std::make_shared<App::Exec::Domain>(
        [](Exec::UpdateScheduler::Scheduler sched) { 
            return MakeMainSender(sched); 
    });
    auto runner = App::Loop::CreateDefaultRunner(domain);
    return runner->Run();
}
