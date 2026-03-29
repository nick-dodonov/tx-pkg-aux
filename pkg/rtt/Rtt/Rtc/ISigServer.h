#pragma once
#include <concepts>
#include <cstdint>

namespace Rtt::Rtc
{

// ---------------------------------------------------------------------------
// ISigServer
// ---------------------------------------------------------------------------

/// A running signaling endpoint that clients can connect to.
class ISigServer
{
public:
    virtual ~ISigServer() = default;

    /// Returns the port the server is listening on.
    [[nodiscard]] virtual uint16_t Port() const = 0;
};

// ---------------------------------------------------------------------------
// Concept
// ---------------------------------------------------------------------------

template <typename T>
concept SigServerLike = requires(const T& t) {
    { t.Port() } -> std::convertible_to<uint16_t>;
};

static_assert(SigServerLike<ISigServer>);

} // namespace Rtt::Rtc
