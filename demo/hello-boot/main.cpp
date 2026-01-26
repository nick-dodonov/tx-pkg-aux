#include "Boot/Boot.h"
#include "Boot/CliArgs.h"
#include "Log/Log.h"
#include "Build/Info.h"

#include "sublib.h"

#include "rules_cc/cc/runfiles/runfiles.h"
using ::rules_cc::cc::runfiles::Runfiles;

int main(const int argc, const char* argv[])
{
    auto args = Boot::DefaultInit(argc, argv);
    Log::Info("Hello Boot!");

    // Display build information
    Log::Info("Build Info:");
    Log::Info("  Git SHA: {}", Build::Info::GitSha());
    Log::Info("  Git Branch: {}", Build::Info::GitBranch());
    Log::Info("  Git Status: {}", Build::Info::GitStatus());
    Log::Info("  Build Time: {}", Build::Info::BuildTime());
    Log::Info("  Build User: {}", Build::Info::BuildUser());
    Log::Info("  Build Host: {}", Build::Info::BuildHost());

    if (args.Contains("--abort")) {
        std::abort();
    }

    if (args.Contains("--throw")) {
        sublib_throw_runtime_exception();
    }

    if (args.Contains("--ex")) {
        const auto ep = sublib_make_exception_ptr();
        sublib_log_exception_ptr(ep);
        return 0;
    }

    // Runfiles example
    {
        std::string error;
        std::unique_ptr<Runfiles> runfiles(Runfiles::Create(argv[0], &error));
        if (runfiles == nullptr) {
            Log::Error("Runfiles::Create failed: {}", error);
            return 1;
        }
        std::string path = runfiles->Rlocation("demo/hello-boot/sample-data.txt");
        Log::Info("Runfiles path for sample: {}", path);
    }

    const auto ec = argc - 1;
    Log::Info("exit status: {}", ec);
    return argc - 1;
}
