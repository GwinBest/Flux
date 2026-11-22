#include "ConnectionManager.hpp"

#include <algorithm>
#include <boost/system/system_error.hpp>
#include <cmath>
#include <exception>
#include <limits>
#include <stdexcept>
#include <vector>

#include "Networking/Connection.hpp"
#include "Utils/Log.hpp"
#include "Utils/SystemResources.hpp"

namespace Flux
{
    namespace
    {
        auto ValidateConnectionOptions(const ConnectionOptions& options) -> void
        {
            if (options.IdleTimeout <= std::chrono::seconds::zero())
                throw std::invalid_argument("IdleTimeout must be greater than zero");
        }

        auto DetectMaxConnections(const ConnectionLimitOptions& options,
                                  const std::optional<std::size_t> maxConnections,
                                  const ConnectionOptions& connectionOptions)
            -> std::expected<std::size_t, std::error_code>
        {
            ValidateConnectionOptions(connectionOptions);

            const auto result = GetSystemResources();

            if (!result.has_value())
                return std::unexpected(result.error());

            return ConnectionManager::CalculateMaxConnections(result.value(),
                                                              options,
                                                              maxConnections);
        }

    }   // namespace

    ConnectionManager::ConnectionManager(const ConnectionLimitOptions& options,
                                         SessionFactory sessionFactory)
        : ConnectionManager(options, ConnectionOptions(), std::nullopt, std::move(sessionFactory))
    {}

    ConnectionManager::ConnectionManager(const ConnectionLimitOptions& limitOptions,
                                         const ConnectionOptions& connectionOptions,
                                         const std::optional<std::size_t> maxConnections,
                                         SessionFactory sessionFactory)
        : _maxConnections(
              DetectMaxConnections(limitOptions, maxConnections, connectionOptions).value())
        , _connectionOptions(connectionOptions)
        , _protocolSessionFactory(std::move(sessionFactory))
    {}

    ConnectionManager::ConnectionManager(const ConnectionOptions& connectionOptions,
                                         const std::optional<std::size_t> maxConnections,
                                         SessionFactory sessionFactory)
        : ConnectionManager(ConnectionLimitOptions(),
                            connectionOptions,
                            maxConnections,
                            std::move(sessionFactory))
    {}

    ConnectionManager::ConnectionManager(const std::size_t maxConnections)
        : ConnectionManager(maxConnections, {})
    {}

    ConnectionManager::ConnectionManager(const std::size_t maxConnections,
                                         const ConnectionOptions& connectionOptions,
                                         SessionFactory sessionFactory)
        : _maxConnections(maxConnections)
        , _connectionOptions(connectionOptions)
        , _protocolSessionFactory(std::move(sessionFactory))
    {
        ValidateConnectionOptions(_connectionOptions);
    }

    auto ConnectionManager::CalculateMaxConnections(const SystemResources& resources,
                                                    const ConnectionLimitOptions& options,
                                                    const std::optional<std::size_t> maxConnections)
        -> std::expected<std::size_t, std::error_code>
    {
        if (options.MemoryPerConnectionBytes == 0 || !std::isfinite(options.SafetyFactor)
            || options.SafetyFactor <= 0.0 || options.SafetyFactor > 1.0)
        {
            return std::unexpected(MakeErrorCode(Error::InvalidArgument));
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

        const auto calculatedLimit = static_cast<std::size_t>(scaledLimit);
        return maxConnections ? std::min(calculatedLimit, *maxConnections) : calculatedLimit;
    }

    auto ConnectionManager::CreateConnection(boost::asio::ip::tcp::socket&& socket) const
        -> std::expected<ConnectionPtr, ErrorCode>
    {
        return CreateConnection(std::move(socket), _connectionOptions.IdleTimeout);
    }

    auto ConnectionManager::CreateConnection(boost::asio::ip::tcp::socket&& socket,
                                             const std::chrono::seconds idleTimeout) const
        -> std::expected<ConnectionPtr, ErrorCode>
    {
        if (idleTimeout <= std::chrono::seconds::zero())
            return std::unexpected(MakeErrorCode(Error::InvalidArgument));

        std::unique_ptr<IProtocolSession> protocol;
        if (_protocolSessionFactory)
        {
            protocol = _protocolSessionFactory();
            if (!protocol)
                WarnLog("SessionFactory returned a null session");
        }
        else
        {
            WarnLog("No session factory configured; connections will accept but not process data");
        }

        const std::scoped_lock lock(_state->Mutex);
        if (_state->Connections.size() >= _maxConnections)
        {
            ErrorCode errorCode;
            errorCode = socket.close(errorCode);
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
            },
            std::move(protocol),
            ConnectionOptions {
                .IdleTimeout = idleTimeout,
                .MaxRequestSize = _connectionOptions.MaxRequestSize,
                .MaxSendQueue = _connectionOptions.MaxSendQueue,
            }));

        _state->Connections.insert(connection);

        return connection;
    }

    auto ConnectionManager::CreateAndStartConnection(boost::asio::ip::tcp::socket&& socket) const
        -> std::expected<ConnectionPtr, ErrorCode>
    {
        return CreateAndStartConnection(std::move(socket), _connectionOptions.IdleTimeout);
    }

    auto ConnectionManager::CreateAndStartConnection(boost::asio::ip::tcp::socket&& socket,
                                                     const std::chrono::seconds idleTimeout) const
        -> std::expected<ConnectionPtr, ErrorCode>
    {
        auto createResult = CreateConnection(std::move(socket), idleTimeout);

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
