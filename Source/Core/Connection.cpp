#include "Connection.hpp"

#include <boost/asio/dispatch.hpp>
#include <boost/asio/error.hpp>
#include <boost/system/system_error.hpp>
#include <new>
#include <system_error>

#include "Utils/Async.hpp"
#include "Utils/Log.hpp"

namespace Flux
{
    auto Connection::AsyncStart() -> std::future<Result>
    {
        return DispatchOperation(_socket.get_executor(),
                                 [self = shared_from_this()] mutable -> Result {
                                     const auto connection = std::move(self);
                                     return connection->Start();
                                 });
    }

    auto Connection::AsyncClose() -> std::future<Result>
    {
        return DispatchOperation(_socket.get_executor(),
                                 [self = shared_from_this()] mutable -> Result {
                                     const auto connection = std::move(self);
                                     return connection->Close();
                                 });
    }

    auto Connection::Send(std::vector<std::byte>&& data) -> std::future<Result>
    {
        return DispatchOperation(
            _socket.get_executor(),
            [self = shared_from_this(), data = std::move(data)] mutable -> Result {
                const auto connection = std::move(self);
                return connection->QueueSend(std::move(data));
            });
    }

    auto Connection::SetIdleTimeout(const std::chrono::seconds idleTimeout) -> void
    {
        _idleTimeout.store(idleTimeout);
        boost::asio::dispatch(_socket.get_executor(), [self = shared_from_this()] -> void {
            if (self->_status == Status::Started)
                self->RestartIdleTimer();
        });
    }

    auto Connection::GetIdleTimeout() const -> std::chrono::seconds
    {
        return _idleTimeout.load();
    }

    Connection::Connection(boost::asio::ip::tcp::socket&& socket,
                           std::function<void(const std::shared_ptr<Connection>&)>&& onClose,
                           const std::chrono::seconds idleTimeout)
        : _socket(std::move(socket))
        , _readBuffer()
        , _onClose(std::move(onClose))
        , _idleTimeout(idleTimeout)
        , _idleTimer(_socket.get_executor())
    {}

    auto Connection::QueueSend(std::vector<std::byte>&& data) -> Result
    {
        if (_status == Status::Closed)
            return std::unexpected(MakeErrorCode(Error::ConnectionClosed));

        if (_status != Status::Started)
            return std::unexpected(MakeErrorCode(Error::ConnectionNotStarted));

        if (data.empty())
            return {};

        _sendQueue.emplace_front(std::make_shared<std::vector<std::byte>>(std::move(data)));
        if (_sendQueue.size() == 1)
        {
            if (const auto result = DoWrite(); !result)
            {
                ErrorLog("Failed to start connection write: {}", result.error().message());
                if (const auto closeResult = Close(); !closeResult)
                {
                    ErrorLog("Failed to close connection after write initiation failure: {}",
                             closeResult.error().message());
                }
                return result;
            }
        }
        return {};
    }

    auto Connection::Start() -> Result
    {
        if (_status == Status::Closed)
            return std::unexpected(MakeErrorCode(Error::ConnectionClosed));

        if (_status != Status::NotInitialized)
            return std::unexpected(MakeErrorCode(Error::ConnectionAlreadyStarted));

        if (!_socket.is_open())
            return std::unexpected(ErrorCode(boost::asio::error::bad_descriptor));

        _status = Status::Started;

        RestartIdleTimer();

        const auto result = DoRead();
        if (!result)
        {
            ErrorLog("Failed to start connection read: {}", result.error().message());
            if (const auto closeResult = Close(); !closeResult)
            {
                ErrorLog("Failed to close connection after read initiation failure: {}",
                         closeResult.error().message());
            }
        }

        return result;
    }

    auto Connection::Close() -> Result
    {
        if (_status == Status::Closed)
            return {};

        ErrorCode error = _socket.close(error);

        _idleTimer.cancel();

        _sendQueue.clear();
        _writeOffset = 0;
        _status = Status::Closed;

        _onClose(shared_from_this());

        if (error)
        {
            ErrorLog("Failed to close connection: {}", error.message());
            return std::unexpected(error);
        }

        return {};
    }

    auto Connection::DoRead() -> Result
    {
        if (_status != Status::Started)
            return std::unexpected(MakeErrorCode(Error::ConnectionNotStarted));

        _socket.async_read_some(
            boost::asio::buffer(_readBuffer),
            [self = shared_from_this()](const ErrorCode& errorCode,
                                        std::size_t /*length*/) -> void {
                if (self->_status != Status::Started)
                    return;

                if (errorCode)
                {
                    WarnLog("Connection async read failed: {}", errorCode.message());
                    if (const auto closeResult = self->Close(); !closeResult)
                    {
                        ErrorLog("Failed to close connection after async read failure: {}",
                                 closeResult.error().message());
                    }
                    return;
                }

                self->RestartIdleTimer();

                // TODO: handle packet

                if (const auto result = self->DoRead(); !result)
                {
                    ErrorLog("Failed to restart connection read: {}", result.error().message());
                    if (const auto closeResult = self->Close(); !closeResult)
                    {
                        ErrorLog("Failed to close connection after read restart failure: {}",
                                 closeResult.error().message());
                    }
                }
            });

        return {};
    }

    // NOLINTNEXTLINE(misc-no-recursion)
    auto Connection::DoWrite() -> Result
    {
        if (_status != Status::Started)
            return std::unexpected(MakeErrorCode(Error::ConnectionNotStarted));

        if (_sendQueue.empty())
            return {};

        const auto data = _sendQueue.back();

        _socket.async_write_some(
            boost::asio::buffer(*data) + _writeOffset,
            // NOLINTNEXTLINE(misc-no-recursion)
            [self = shared_from_this(), data](const ErrorCode& errorCode,
                                              const std::size_t length) -> void {
                if (self->_status != Status::Started)
                    return;

                if (errorCode)
                {
                    WarnLog("Connection async write failed: {}", errorCode.message());
                    if (const auto closeResult = self->Close(); !closeResult)
                    {
                        ErrorLog("Failed to close connection after async write failure: {}",
                                 closeResult.error().message());
                    }
                    return;
                }

                if (length > 0)
                {
                    self->_writeOffset += length;
                    self->RestartIdleTimer();
                }

                if (self->_writeOffset == data->size())
                {
                    self->_sendQueue.pop_back();
                    self->_writeOffset = 0;
                }

                if (const auto result = self->DoWrite(); !result)
                {
                    ErrorLog("Failed to start next connection write: {}", result.error().message());
                    if (const auto closeResult = self->Close(); !closeResult)
                    {
                        ErrorLog(
                            "Failed to close connection after next write initiation failure: {}",
                            closeResult.error().message());
                    }
                }
            });
        return {};
    }

    auto Connection::RestartIdleTimer() -> void
    {
        _idleTimer.expires_after(GetIdleTimeout());
        const auto generation = ++_idleTimerGeneration;
        _idleTimer.async_wait(
            [self = shared_from_this(), generation](const ErrorCode& timerError) -> void {
                if (self->_status != Status::Started || generation != self->_idleTimerGeneration)
                    return;

                if (timerError == boost::asio::error::operation_aborted)
                {
                    return;
                }

                if (!timerError)
                {
                    InfoLog("[Client] Timed out due to inactivity. Closing connection.\n");
                    if (const auto closeResult = self->Close(); !closeResult)
                    {
                        ErrorLog("Failed to close connection after idle timeout: {}",
                                 closeResult.error().message());
                    }
                }
            });
    }
}   // namespace Flux
