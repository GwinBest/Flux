#include "Connection.hpp"

#include <boost/asio/dispatch.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/system_error.hpp>
#include <new>
#include <system_error>

#include "Utils/Log.hpp"

namespace Flux
{
    namespace
    {
        template<typename Operation>
        auto DispatchOperation(const boost::asio::any_io_executor& executor, Operation&& operation)
            -> std::future<Result>
        {
            std::packaged_task<Result()> task(std::forward<Operation>(operation));
            auto future = task.get_future();

            boost::asio::dispatch(executor, [task = std::move(task)] mutable -> void { task(); });
            return future;
        }
    }   // namespace

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

        _sendQueue.clear();
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

    auto Connection::DoWrite() -> Result
    {
        if (_status != Status::Started)
            return std::unexpected(MakeErrorCode(Error::ConnectionNotStarted));

        if (_sendQueue.empty())
            return {};

        const auto data = _sendQueue.back();

        boost::asio::async_write(
            _socket,
            boost::asio::buffer(*data),
            // Data is captured to keep the active buffer alive even if Close() clears the queue.
            [self = shared_from_this(), data](const ErrorCode& errorCode,
                                              [[maybe_unused]] std::size_t length) -> void {
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

                self->_sendQueue.pop_back();

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
}   // namespace Flux
