#pragma once
#include "PeerId.h"
#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <span>

namespace Rtt
{
    /// Writer callback passed to Send(). The transport invokes it with a
    /// writable buffer; the user writes payload data and returns the number
    /// of bytes actually written. This allows implementations to hand out
    /// ring-buffer or packet-buffer slices for true zero-copy sends.
    using WriteCallback = std::function<std::size_t(std::span<std::byte>)>;

    /// Compile-time contract for link-like types.
    template <typename T>
    concept LinkLike = requires(T& t, WriteCallback writer) {
        { t.LocalId() }  -> std::convertible_to<const PeerId&>;
        { t.RemoteId() } -> std::convertible_to<const PeerId&>;
        { t.Send(std::move(writer)) };
        { t.Disconnect() };
    };

    /// Runtime-polymorphic link interface.
    ///
    /// Represents one established connection between two peers. Shared
    /// ownership via shared_ptr because both the user and the transport
    /// hold references to the link.
    class ILink : public std::enable_shared_from_this<ILink>
    {
    public:
        virtual ~ILink() = default;

        /// Identity of the local endpoint on this link.
        [[nodiscard]] virtual const PeerId& LocalId() const = 0;

        /// Identity of the remote endpoint on this link.
        [[nodiscard]] virtual const PeerId& RemoteId() const = 0;

        /// Enqueue a send operation. The transport will invoke `writer` with a
        /// writable buffer when ready; `writer` returns the number of bytes
        /// written. Fire-and-forget: no completion notification.
        virtual void Send(WriteCallback writer) = 0;

        /// Initiate a graceful disconnect.
        virtual void Disconnect() = 0;
    };

    static_assert(LinkLike<ILink>);
}
