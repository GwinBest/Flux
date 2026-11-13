#pragma once

#include <boost/beast/core.hpp>
#include <cstddef>
#include <functional>
#include <memory>
#include <span>

namespace Flux
{
    class IProtocolSession
    {
    public:
        IProtocolSession() = default;
        IProtocolSession(const IProtocolSession&) = default;
        auto operator=(const IProtocolSession&) -> IProtocolSession& = default;
        IProtocolSession(IProtocolSession&&) = default;
        auto operator=(IProtocolSession&&) -> IProtocolSession& = default;
        virtual ~IProtocolSession() = default;

        virtual auto OnBytes(const std::span<const std::byte> bytes) -> bool = 0;

        auto SetMaxRequestSize(const std::size_t maxRequestSize) noexcept -> void
        {
            _leftoverBuffer.max_size(maxRequestSize);
        }

        [[nodiscard]] auto GetMaxRequestSize() const noexcept -> std::size_t
        {
            return _leftoverBuffer.max_size();
        }

    protected:
        boost::beast::flat_buffer _leftoverBuffer;
    };

    using SessionFactory = std::function<std::unique_ptr<IProtocolSession>()>;

}   // namespace Flux
