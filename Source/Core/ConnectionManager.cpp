#include "ConnectionManager.hpp"

#include "Utils/Log.hpp"

namespace Flux
{
    auto ConnectionManager::CreateConnection(boost::asio::ip::tcp::socket socket)
        -> ConnectionResult
    {
        auto newConnection = std::shared_ptr<Connection>(new Connection(std::move(socket)));

        if (_connections.contains(newConnection))
            return std::unexpected(MakeErrorCode(Error::ConnectionAlreadyStarted));

        _connections.emplace(newConnection);

        return newConnection;
    }

    auto ConnectionManager::CreateAndStartConnection(boost::asio::ip::tcp::socket socket,
                                                     Connection::CompletionHandler onStarted)
        -> ConnectionResult
    {
        auto newConnection = CreateConnection(std::move(socket));

        if (!newConnection.has_value())
            return newConnection;

        if (const auto result = newConnection.value()->StartAsync(std::move(onStarted));
            !result.has_value())
        {
            _connections.erase(*newConnection);
            return std::unexpected(result.error());
        }

        return newConnection;
    }

    auto ConnectionManager::CloseConnection(const ConnectionPtr& connection,
                                            Connection::CompletionHandler onClosed) -> Result
    {
        const auto iterator = _connections.find(connection);
        if (iterator == _connections.end())
            return std::unexpected(MakeErrorCode(Error::ConnectionNotFound));

        if (const auto result = iterator->get()->CloseAsync(std::move(onClosed));
            !result.has_value())
            return result;

        _connections.erase(iterator);

        return {};
    }

    auto ConnectionManager::CloseAllConnections() -> Result
    {
        Result result;

        for (auto iterator = _connections.begin(); iterator != _connections.end();)
        {
            if (const auto closeResult = iterator->get()->CloseAsync(); !closeResult.has_value())
            {
                if (result.has_value())
                    result = closeResult;

                ++iterator;
            }
            else
            {
                iterator = _connections.erase(iterator);
            }
        }

        return result;
    }
}   // namespace Flux
