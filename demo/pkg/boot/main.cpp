#include "Boot/Boot.h"
#include "Boot/CliArgs.h"
#include "Log/Log.h"

#include "sublib.h"

#include "spdlog/details/os.h"

#include "rules_cc/cc/runfiles/runfiles.h"
using ::rules_cc::cc::runfiles::Runfiles;

int main(const int argc, const char* argv[])
{
    auto args = Boot::DefaultInit(argc, argv);
    Log::Info("Hello Boot!");
    
    if (args.Contains("--break")) {
#if defined(_MSC_VER)
        __debugbreak();
#else
        __builtin_debugtrap();
#endif
    }

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

    // Log environment vars
    {
        Log::Info("Process environment variables:");
        const char* vars[] = {
            "BUILD_WORKING_DIRECTORY",
            "BUILD_WORKSPACE_DIRECTORY",
            "RUNFILES_DIR",
            "RUNFILES_MANIFEST_FILE",
        };
        for (const char* var : vars) {
            auto value = spdlog::details::os::getenv(var);
            if (!value.empty()) {
                Log::Info("  {}={}", var, value);
            } else {
                Log::Info("  {} is not set", var);
            }
        }
    }

    // Runfiles example
    {
        std::string error;
        std::unique_ptr<Runfiles> runfiles(Runfiles::Create(argv[0], &error));
        if (runfiles) {
            std::string path = runfiles->Rlocation("demo/hello-boot/sample-data.txt");
            Log::Info("Runfiles path for sample: {}", path);
        } else {
            // it's OK in WASM build
            Log::Warn("Runfiles::Create failed: {}", error);
        }
    }

    const auto ec = argc - 1;
    Log::Info("exit status: {}", ec);
    return argc - 1;
}
