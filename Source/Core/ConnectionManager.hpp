#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <expected>
#include <memory>
#include <mutex>
#include <unordered_set>

#include "Utils/Error.hpp"

namespace Flux
{
    class Connection;

    class ConnectionManager final
    {
    public:
        using ConnectionPtr = std::shared_ptr<Connection>;

    public:
        ConnectionManager() = default;
        ConnectionManager(const ConnectionManager&) = delete;
        ConnectionManager(ConnectionManager&&) = delete;
        auto operator=(const ConnectionManager&) -> ConnectionManager& = delete;
        auto operator=(ConnectionManager&&) -> ConnectionManager& = delete;
        ~ConnectionManager() = default;

        auto CreateConnection(boost::asio::ip::tcp::socket&& socket) const -> ConnectionPtr;
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
    };
}   // namespace Flux
