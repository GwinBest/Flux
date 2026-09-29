#include "ConnectionManager.hpp"

#include <boost/system/system_error.hpp>
#include <exception>
#include <vector>

#include "Core/Connection.hpp"
#include "Utils/Log.hpp"

namespace Flux
{
    auto ConnectionManager::CreateConnection(boost::asio::ip::tcp::socket&& socket) const
        -> ConnectionPtr
    {
        auto connection = ConnectionPtr(new Connection(
            std::move(socket),
            [weakState = std::weak_ptr(_state)](const ConnectionPtr& connectionToClose) -> void {
                if (const auto state = weakState.lock())
                {
                    const std::scoped_lock lock(state->Mutex);
                    state->Connections.erase(connectionToClose);
                }
            }));

        const std::scoped_lock lock(_state->Mutex);
        _state->Connections.insert(connection);

        return connection;
    }

    auto ConnectionManager::CreateAndStartConnection(boost::asio::ip::tcp::socket&& socket) const
        -> std::expected<ConnectionPtr, ErrorCode>
    {
        auto connection = CreateConnection(std::move(socket));

        auto result = connection->AsyncStart();

        if (const auto startResult = result.get(); !startResult)
        {
            ErrorLog("Failed to start managed connection: {}", startResult.error().message());

            const std::scoped_lock lock(_state->Mutex);
            _state->Connections.erase(connection);

            return std::unexpected(startResult.error());
        }

        return connection;
    }

    auto ConnectionManager::CloseConnection(const ConnectionPtr& connection) const -> Result
    {
        {
            const std::scoped_lock lock(_state->Mutex);
            if (!_state->Connections.contains(connection))
                return {};
        }

        auto result = connection->AsyncClose();

        if (const auto closeResult = result.get(); !closeResult)
        {
            ErrorLog("Failed to close managed connection: {}", closeResult.error().message());
            return closeResult;
        }

        return {};
    }

    auto ConnectionManager::CloseAllConnections() const -> Result
    {
        std::vector<ConnectionPtr> connectionsToClose;
        {
            const std::scoped_lock lock(_state->Mutex);
            connectionsToClose.assign(_state->Connections.begin(), _state->Connections.end());
        }

        ErrorCode firstError;

        for (const auto& connection : connectionsToClose)
        {
            try
            {
                auto result = connection->AsyncClose();
                if (const auto closeResult = result.get(); !closeResult)
                {
                    if (!firstError)
                        firstError = closeResult.error();

                    ErrorLog("Failed to close managed connection during shutdown: {}",
                             closeResult.error().message());
                }
            }
            catch (const boost::system::system_error& error)
            {
                if (!firstError)
                    firstError = error.code();

                ErrorLog("Managed connection close threw during shutdown: {}", error.what());
            }
            catch (const std::exception& error)
            {
                if (!firstError)
                    firstError = MakeErrorCode(Error::ConnectionException);

                ErrorLog("Managed connection close threw during shutdown: {}", error.what());
            }
            catch (...)
            {
                if (!firstError)
                    firstError = MakeErrorCode(Error::ConnectionException);

                ErrorLog("Managed connection close threw an unknown exception during shutdown");
            }
        }

        if (firstError)
            return std::unexpected(firstError);

        return {};
    }
}   // namespace Flux
