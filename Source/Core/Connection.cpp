#include "Connection.hpp"

#include <boost/asio/dispatch.hpp>
#include <boost/asio/error.hpp>
#include <boost/system/system_error.hpp>
#include <new>
#include <span>
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

    auto Connection::SetMaxRequestSize(const std::size_t maxRequestSize) -> void
    {
        _maxRequestSize.store(maxRequestSize);
    }

    auto Connection::GetMaxRequestSize() const -> std::size_t
    {
        return _maxRequestSize.load();
    }

    auto Connection::SetMaxSendQueue(const std::size_t maxSendQueue) -> void
    {
        _maxSendQueue.store(maxSendQueue);
    }

    auto Connection::GetMaxSendQueue() const -> std::size_t
    {
        return _maxSendQueue.load();
    }

    auto Connection::SetIdleTimeout(const std::chrono::seconds idleTimeout) -> Result
    {
        if (idleTimeout <= std::chrono::seconds::zero())
            return std::unexpected(boost::asio::error::invalid_argument);

        _idleTimeout.store(idleTimeout);
        boost::asio::dispatch(_socket.get_executor(), [self = shared_from_this()] -> void {
            if (self->_status == Status::Started)
                self->RestartIdleTimer();
        });
        return {};
    }

    auto Connection::GetIdleTimeout() const -> std::chrono::seconds
    {
        return _idleTimeout.load();
    }

    Connection::Connection(boost::asio::ip::tcp::socket&& socket,
                           std::function<void(const std::shared_ptr<Connection>&)>&& onClose,
                           std::unique_ptr<IProtocolSession> protocol,
                           const ConnectionOptions& options)
        : _socket(std::move(socket))
        , _readBuffer()
        , _maxRequestSize(options.MaxRequestSize)
        , _maxSendQueue(options.MaxSendQueue)
        , _onClose(std::move(onClose))
        , _idleTimeout(options.IdleTimeout)
        , _idleTimer(_socket.get_executor())
        , _protocol(std::move(protocol))
    {}

    auto Connection::QueueSend(std::vector<std::byte>&& data) -> Result
    {
        if (_status == Status::Closed)
            return std::unexpected(MakeErrorCode(Error::ConnectionClosed));

        if (_status != Status::Started)
            return std::unexpected(MakeErrorCode(Error::ConnectionNotStarted));

        if (data.empty())
            return {};

        if (const auto maxSendQueue = GetMaxSendQueue();
            _pendingSendBytes + data.size() > maxSendQueue)
        {
            ErrorLog("Send queue limit exceeded: {} bytes, limit: {}",
                     _pendingSendBytes + data.size(),
                     maxSendQueue);

            return std::unexpected(MakeErrorCode(Error::SendQueueFull));

            // TODO: return http 503
        }

        _sendQueue.emplace_front(std::make_shared<std::vector<std::byte>>(std::move(data)));
        _pendingSendBytes += _sendQueue.front()->size();

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

        ErrorCode error;
        error = _socket.close(error);

        _idleTimer.cancel();

        _sendQueue.clear();
        _pendingSendBytes = 0;
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

        _socket.async_read_some(boost::asio::buffer(_readBuffer),
                                [self = shared_from_this()](const ErrorCode& errorCode,
                                                            const std::size_t length) -> void {
                                    self->OnRead(errorCode, length);
                                });

        return {};
    }

    auto Connection::OnRead(const ErrorCode& errorCode, const std::size_t length) -> void
    {
        if (_status != Status::Started)
            return;

        if (errorCode)
        {
            WarnLog("Connection async read failed: {}", errorCode.message());
            CloseAfterFailure("async read failure");
            return;
        }

        if (!ProcessRead(length))
            return;

        RestartIdleTimer();

        if (const auto result = DoRead(); !result)
        {
            ErrorLog("Failed to restart connection read: {}", result.error().message());
            CloseAfterFailure("read restart failure");
        }
    }

    auto Connection::ProcessRead(const std::size_t length) -> bool
    {
        const auto limit = GetMaxRequestSize();
        if (length > limit)
        {
            ErrorLog("Max request size exceeded! Current: {}, Limit: {}", length, limit);
            CloseAfterFailure("request size limit exceeded");

            // TODO: return http 413

            return false;
        }

        if (!_protocol)
            return true;

        _protocol->SetMaxRequestSize(limit);
        const auto received = std::span(_readBuffer).first(length);
        if (_protocol->OnBytes(received))
            return true;

        CloseAfterFailure("protocol rejected input");
        return false;
    }

    auto Connection::CloseAfterFailure(const std::string_view reason) -> void
    {
        if (const auto closeResult = Close(); !closeResult)
        {
            ErrorLog("Failed to close connection after {}: {}",
                     reason,
                     closeResult.error().message());
        }
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
            [self = shared_from_this(), data](const ErrorCode& errorCode, const std::size_t length)
                -> void { self->OnWrite(data, errorCode, length); });
        return {};
    }

    auto Connection::OnWrite(const std::shared_ptr<std::vector<std::byte>>& data,
                             const ErrorCode& errorCode,
                             const std::size_t length) -> void
    {
        if (_status != Status::Started)
            return;

        if (errorCode)
        {
            WarnLog("Connection async write failed: {}", errorCode.message());
            CloseAfterFailure("async write failure");
            return;
        }

        if (length > 0)
        {
            _writeOffset += length;
            RestartIdleTimer();
        }

        if (_writeOffset == data->size())
        {
            _pendingSendBytes -= data->size();
            _sendQueue.pop_back();
            _writeOffset = 0;
        }

        if (const auto result = DoWrite(); !result)
        {
            ErrorLog("Failed to start next connection write: {}", result.error().message());
            CloseAfterFailure("next write initiation failure");
        }
    }

    auto Connection::RestartIdleTimer() -> void
    {
        const auto idleTimeout = GetIdleTimeout();
        const auto maximumTimeout = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::time_point::max() - std::chrono::steady_clock::now());
        _idleTimer.expires_after(idleTimeout < maximumTimeout ? idleTimeout : maximumTimeout);
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
