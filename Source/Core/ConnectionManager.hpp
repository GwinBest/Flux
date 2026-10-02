#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <unordered_set>

#include "Utils/Error.hpp"
#include "Utils/SystemResources.hpp"

namespace Flux
{
    class Connection;

    struct ConnectionLimitOptions final
    {
        static constexpr std::uint64_t DefaultMemoryPerConnectionBytes = 256ULL * 1024;
        static constexpr std::uint64_t DefaultReservedFileDescriptors = 64;
        static constexpr double DefaultSafetyFactor = 0.8F;

        std::uint64_t MemoryPerConnectionBytes = DefaultMemoryPerConnectionBytes;
        std::uint64_t ReservedFileDescriptors = DefaultReservedFileDescriptors;
        double SafetyFactor = DefaultSafetyFactor;
    };

    class ConnectionManager final
    {
    public:
        using ConnectionPtr = std::shared_ptr<Connection>;

    public:
        explicit ConnectionManager(const ConnectionLimitOptions& options = {});
        explicit ConnectionManager(std::size_t maxConnections);
        ConnectionManager(const ConnectionManager&) = delete;
        ConnectionManager(ConnectionManager&&) = delete;
        auto operator=(const ConnectionManager&) -> ConnectionManager& = delete;
        auto operator=(ConnectionManager&&) -> ConnectionManager& = delete;
        ~ConnectionManager() = default;

        [[nodiscard]] auto GetMaxConnections() const noexcept -> std::size_t
        {
            return _maxConnections;
        }

        [[nodiscard]] static auto
            CalculateMaxConnections(const SystemResources& resources,
                                    const ConnectionLimitOptions& options = {})
                -> std::expected<std::size_t, std::error_code>;

        auto CreateConnection(boost::asio::ip::tcp::socket&& socket) const
            -> std::expected<ConnectionPtr, ErrorCode>;
        auto CreateAndStartConnection(boost::asio::ip::tcp::socket&& socket) const
            -> std::expected<ConnectionPtr, ErrorCode>;

        [[nodiscard]] auto CloseConnection(const ConnectionPtr& connection) const -> Result;
        [[nodiscard]] auto CloseAllConnections() const -> Result;

    private:
        struct State final
        {
            std::unordered_set<ConnectionPtr> Connections;
            std::mutex Mutex;
        };

        std::shared_ptr<State> _state = std::make_shared<State>();

        const std::size_t _maxConnections;
    };
}   // namespace Flux
