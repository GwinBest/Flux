#pragma once

#include <memory>
#include <set>

#include "Connection.hpp"
#include "Utils/Error.hpp"

namespace Flux
{
    class ConnectionManager final
    {
        using ConnectionPtr = std::shared_ptr<Connection>;
        using ConnectionResult = std::expected<ConnectionPtr, ErrorCode>;

    public:
        auto CreateConnection(boost::asio::ip::tcp::socket socket) -> ConnectionResult;
        auto CreateAndStartConnection(boost::asio::ip::tcp::socket socket,
                                      Connection::CompletionHandler onStarted = {}) -> ConnectionResult;

        // Manager calls must be serialized by the caller. Results report scheduling;
        // connection callbacks report completion on each connection's strand.
        auto CloseConnection(const ConnectionPtr& connection,
                             Connection::CompletionHandler onClosed = {}) -> Result;
        auto CloseAllConnections() -> Result;

    private:
        std::set<ConnectionPtr> _connections;
    };

}   // namespace Flux
