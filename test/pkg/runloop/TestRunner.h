#pragma once

#include "RunLoop/Runner.h"

#include <memory>
#include <optional>

namespace
{
    /// Minimal runner for unit tests that captures exit codes without an event loop.
    ///
    /// Exposes Drive* helpers that call the protected Runner invoke methods, so tests
    /// can exercise the full Start / Update / Stop lifecycle without knowing about
    /// runner internals.
    ///
    ///   TestRunner runner{domain};
    ///   runner.DriveStart();
    ///   runner.DriveUpdate();   // repeat as needed
    ///   runner.DriveStop();
    ///   EXPECT_EQ(runner.exitCode, RunLoop::ExitCode::Cancelled);
    struct TestRunner: RunLoop::Runner
    {
        explicit TestRunner(std::shared_ptr<RunLoop::Handler> handler)
            : Runner{std::move(handler)}
        {}

        // IRunner
        int Run() override { return exitCode.value_or(-1); }
        void Exit(int code) override
        {
            if (!exitCode) // never overwrite: first caller wins
                exitCode = code;
        }
        std::optional<int> Exiting() const override { return exitCode; }

        // Drive helpers
        bool DriveStart() { return InvokeStart(); }
        void DriveUpdate(uint64_t frameIndex = 0)
        {
            RunLoop::UpdateCtx ctx{*this};
            ctx.frame.index = frameIndex;
            InvokeUpdate(ctx);
        }
        void DriveStop() { InvokeStop(); }

        std::optional<int> exitCode;
    };
}
