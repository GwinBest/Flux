#include "ConnectionManager.hpp"

#include <algorithm>
#include <boost/system/system_error.hpp>
#include <cmath>
#include <exception>
#include <limits>
#include <stdexcept>
#include <vector>

#include "Core/Connection.hpp"
#include "Utils/Log.hpp"
#include "Utils/SystemResources.hpp"

namespace Flux
{
    namespace
    {
        auto DetectMaxConnections(const ConnectionLimitOptions& options)
            -> std::expected<std::size_t, std::error_code>
        {
            const auto result = GetSystemResources();

            if (!result.has_value())
                return std::unexpected(result.error());

            return ConnectionManager::CalculateMaxConnections(result.value(), options);
        }
    }   // namespace

    ConnectionManager::ConnectionManager(const ConnectionLimitOptions& options)
        : ConnectionManager(DetectMaxConnections(options).value())
    {}

    ConnectionManager::ConnectionManager(const std::size_t maxConnections)
        : _maxConnections(maxConnections)
    {}

    auto ConnectionManager::CalculateMaxConnections(const SystemResources& resources,
                                                    const ConnectionLimitOptions& options)
        -> std::expected<std::size_t, std::error_code>
    {
        if (options.MemoryPerConnectionBytes == 0 || !std::isfinite(options.SafetyFactor)
            || options.SafetyFactor <= 0.0 || options.SafetyFactor > 1.0)
        {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }

        const auto osLimit = resources.FileDescriptorLimit > options.ReservedFileDescriptors
                               ? resources.FileDescriptorLimit - options.ReservedFileDescriptors
                               : 0;
        const auto memoryLimit = resources.AvailableMemoryBytes / options.MemoryPerConnectionBytes;
        const auto scaledLimit = static_cast<long double>(std::min(osLimit, memoryLimit))
                               * static_cast<long double>(options.SafetyFactor);

        // Guard before conversion: on MSVC even long double may round SIZE_MAX upward.
        if (constexpr auto maxSize = std::numeric_limits<std::size_t>::max();
            scaledLimit >= static_cast<long double>(maxSize))
        {
            return maxSize;
        }

        return static_cast<std::size_t>(scaledLimit);
    }

    auto ConnectionManager::CreateConnection(boost::asio::ip::tcp::socket&& socket) const
        -> std::expected<ConnectionPtr, ErrorCode>
    {
        const std::scoped_lock lock(_state->Mutex);
        if (_state->Connections.size() >= _maxConnections)
        {
            ErrorCode errorCode = socket.close(errorCode);
            return std::unexpected(MakeErrorCode(Error::ConnectionLimitReached));

            // TODO: return http 503
        }

        auto connection = ConnectionPtr(new Connection(
            std::move(socket),
            [weakState = std::weak_ptr(_state)](const ConnectionPtr& connectionToClose) -> void {
                if (const auto state = weakState.lock())
                {
                    const std::scoped_lock closeLock(state->Mutex);
                    state->Connections.erase(connectionToClose);
                }
            }));

        _state->Connections.insert(connection);

        return connection;
    }

    auto ConnectionManager::CreateAndStartConnection(boost::asio::ip::tcp::socket&& socket) const
        -> std::expected<ConnectionPtr, ErrorCode>
    {
        auto createResult = CreateConnection(std::move(socket));

        if (!createResult.has_value())
            return std::unexpected(createResult.error());

        ConnectionPtr connection = createResult.value();

        try
        {
            auto result = connection->AsyncStart();

            if (const auto startResult = result.get(); !startResult)
            {
                ErrorLog("Failed to start managed connection: {}", startResult.error().message());

                const std::scoped_lock lock(_state->Mutex);
                _state->Connections.erase(connection);

                return std::unexpected(startResult.error());
            }
        }
        catch (...)
        {
            const std::scoped_lock lock(_state->Mutex);
            _state->Connections.erase(connection);
            throw;
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
