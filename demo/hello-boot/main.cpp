#include "Boot/Boot.h"
#include "Boot/CliArgs.h"
#include "Log/Log.h"

#include "sublib.h"

int main(const int argc, const char* argv[])
{
    const Boot::CliArgs args{argc, argv};
    Boot::LogHeader(args);
    Boot::SetupAbortHandlers();

    Log::Info("Hello Boot!");

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

    const auto ec = argc - 1;
    Log::Info("exiting: {}", ec);
    return argc - 1;
}
