#include "App/Loop/Factory.h"
#include "Boot/Boot.h"
#include "Log/Log.h"
#include "Exec/Exec.h"

using namespace std::literals;

class Domain: public App::Loop::Handler
{
public:
    bool Start() override
    {
        Log::Info("Domain started");
        return true;
    }

    void Stop() override { Log::Info("Domain stopped"); }

    void Update(const App::Loop::UpdateCtx& ctx) override
    {
        Log::Info("frame {} - {}µs", ctx.frame.index, std::chrono::duration_cast<std::chrono::microseconds>(ctx.frame.delta).count());
        if (ctx.frame.index == 5) {
            Log::Info("exiting loop");
            GetRunner()->Exit(42);
        }
    }
};

int main(const int argc, const char* argv[])
{
    Boot::DefaultInit(argc, argv);
    auto domain = std::make_shared<Domain>();
    auto runner = App::Loop::CreateDefaultRunner(domain);
    return runner->Run();
}
