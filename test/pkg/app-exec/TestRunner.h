#pragma once

#include "App/Loop/Runner.h"

#include <memory>
#include <optional>

namespace {

/// Minimal runner that captures the exit code without running an actual loop.
/// Lets tests drive Domain lifecycle (Start / Stop) manually. When constructed,
/// it registers itself with the handler via Runner::Runner(), so GetRunner()
/// inside OnStopped/OnComplete is valid.
struct TestRunner : App::Loop::Runner
{
    explicit TestRunner(std::shared_ptr<App::Loop::Handler> handler)
        : Runner{std::move(handler)}
    {}

    int Run() override { return exitCode.value_or(-1); }
    void Exit(int code) override { exitCode = code; }

    std::optional<int> exitCode;
};

} // namespace
