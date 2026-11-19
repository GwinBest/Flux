#pragma once

#include <boost/beast/core.hpp>
#include <cstddef>
#include <functional>
#include <span>

#include "Utils/Error.hpp"

namespace Flux
{
    class IProtocolSession
    {
    public:
        using SendCallback = std::function<Result(std::vector<std::byte>&&)>;

    public:
        IProtocolSession() = default;
        IProtocolSession(const IProtocolSession&) = default;
        auto operator=(const IProtocolSession&) -> IProtocolSession& = default;
        IProtocolSession(IProtocolSession&&) = default;
        auto operator=(IProtocolSession&&) -> IProtocolSession& = default;
        virtual ~IProtocolSession() = default;

        virtual auto OnBytes(const std::span<const std::byte> bytes) -> bool = 0;

        auto SetMaxRequestSize(const std::size_t maxRequestSize) noexcept -> void;

        [[nodiscard]] auto GetMaxRequestSize() const noexcept -> std::size_t;

        auto SetSendCallback(const SendCallback& sendCallback) noexcept -> void;

    protected:
        auto BufferIncomingBytes(const std::span<const std::byte> bytes) -> bool;

        auto SendToClient(std::vector<std::byte>&& data) const noexcept -> Result;

    protected:
        boost::beast::flat_buffer _leftoverBuffer;

        SendCallback _sendCallback;
    };

    using SessionFactory = std::function<std::unique_ptr<IProtocolSession>()>;

}   // namespace Flux
