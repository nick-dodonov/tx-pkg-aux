#pragma once
#include <stdexec/execution.hpp>

namespace ExecTest {

struct Outcome
{
    bool valued  = false;
    bool stopped = false;
};

/// Receiver with no stop token — never-stop fallback.
struct BasicReceiver
{
    using receiver_concept = stdexec::receiver_t;

    Outcome* outcome;

    [[nodiscard]] auto get_env() const noexcept { return stdexec::env<>{}; }
    void set_value() const noexcept   { outcome->valued  = true; }
    void set_stopped() const noexcept { outcome->stopped = true; }
    void set_error(auto&&) noexcept {}
};

/// Environment that provides a concrete inplace_stop_token.
struct StopEnv
{
    stdexec::inplace_stop_token token;

    [[nodiscard]] auto query(stdexec::get_stop_token_t) const noexcept { return token; }
};

/// Receiver that exposes a stop token — needed for set_stopped tests.
struct StopReceiver
{
    using receiver_concept = stdexec::receiver_t;

    Outcome*                    outcome;
    stdexec::inplace_stop_token token;

    [[nodiscard]] auto get_env() const noexcept { return StopEnv{token}; }
    void set_value() const noexcept   { outcome->valued  = true; }
    void set_stopped() const noexcept { outcome->stopped = true; }
    void set_error(auto&&) noexcept {}
};

} // namespace ExecTest
