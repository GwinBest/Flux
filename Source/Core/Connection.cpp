#include "Connection.hpp"

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/system_error.hpp>

#include "Utils/Log.hpp"

namespace Flux
{
    auto Connection::StartAsync(CompletionHandler onStarted) -> Result
    {
        try
        {
            boost::asio::post(
                _strand,
                [self = shared_from_this(), onStarted = std::move(onStarted)] -> void {
                    const auto result = self->Start();

                    if (onStarted)
                        onStarted(result);
                    else if (!result)
                        WarnLog("Connection start failed: {}", result.error().message());
                });
        }
        catch (const boost::system::system_error& error)
        {
            return std::unexpected(error.code());
        }
        return {};
    }

    auto Connection::SendAsync(std::vector<std::byte> data, CompletionHandler onQueued) -> Result
    {
        try
        {
            boost::asio::post(_strand,
                              [self = shared_from_this(),
                               data = std::move(data),
                               onQueued = std::move(onQueued)] -> void {
                                  const auto result = self->Send(data);

                                  if (onQueued)
                                      onQueued(result);
                                  else if (!result)
                                      WarnLog("Connection send failed: {}",
                                              result.error().message());
                              });
        }
        catch (const boost::system::system_error& error)
        {
            return std::unexpected(error.code());
        }
        return {};
    }

    auto Connection::CloseAsync(CompletionHandler onClosed) -> Result
    {
        try
        {
            boost::asio::post(_strand,
                              [self = shared_from_this(), onClosed = std::move(onClosed)] -> void {
                                  const auto result = self->Close();

                                  if (onClosed)
                                      onClosed(result);
                                  else if (!result)
                                      WarnLog("Connection close failed: {}",
                                              result.error().message());
                              });
        }
        catch (const boost::system::system_error& error)
        {
            return std::unexpected(error.code());
        }

        return {};
    }

    auto Connection::Start() -> Result
    {
        if (_state != State::NotStarted)
            return std::unexpected(MakeErrorCode(Error::ConnectionAlreadyStarted));

        if (!_socket.is_open())
            return std::unexpected(ErrorCode(boost::asio::error::bad_descriptor));

        _state = State::Started;

        if (const auto result = DoRead(); !result.has_value())
        {
            _state = State::NotStarted;
            return result;
        }

        return {};
    }

    auto Connection::Send(std::vector<std::byte> data) -> Result
    {
        if (_state != State::Started)
            return std::unexpected(MakeErrorCode(Error::ConnectionNotStarted));

        if (data.empty())
            return {};

        _writeQueue.push_back(std::make_shared<std::vector<std::byte>>(std::move(data)));

        if (_writeQueue.size() == 1)
        {
            if (const auto result = DoWrite(); !result.has_value())
            {
                _writeQueue.pop_back();
                return result;
            }
        }

        return {};
    }

    auto Connection::DoRead() -> Result
    {
        if (_state != State::Started)
            return std::unexpected(MakeErrorCode(Error::ConnectionNotStarted));

        try
        {
            _socket.async_read_some(
                boost::asio::buffer(_readBuffer),
                boost::asio::bind_executor(
                    _strand,
                    [self = shared_from_this()](const ErrorCode& errorCode, std::size_t) -> void {
                        if (self->_state != State::Started)
                            return;

                        if (errorCode)
                        {
                            if (errorCode != boost::asio::error::eof)
                                WarnLog("Connection read failed: {}", errorCode.message());

                            if (const auto closeResult = self->Close(); !closeResult.has_value())
                                WarnLog("Connection close failed: {}",
                                        closeResult.error().message());

                            return;
                        }

                        if (const auto result = self->DoRead(); !result.has_value())
                        {
                            WarnLog("Connection read failed: {}", result.error().message());

                            if (const auto closeResult = self->Close(); !closeResult.has_value())
                                WarnLog("Connection close failed: {}",
                                        closeResult.error().message());
                        }
                    }));
        }
        catch (const boost::system::system_error& error)
        {
            return std::unexpected(error.code());
        }

        return {};
    }

    auto Connection::DoWrite() -> Result
    {
        if (_state != State::Started)
            return std::unexpected(MakeErrorCode(Error::ConnectionNotStarted));

        if (_writeQueue.empty())
            return {};

        const auto data = _writeQueue.front();
        try
        {
            boost::asio::async_write(
                _socket,
                boost::asio::buffer(*data),
                boost::asio::bind_executor(
                    _strand,
                    [self = shared_from_this()](const ErrorCode& errorCode, std::size_t) -> void {
                        if (self->_state != State::Started)
                            return;

                        if (errorCode)
                        {
                            WarnLog("Connection write failed: {}", errorCode.message());

                            if (const auto closeResult = self->Close(); !closeResult.has_value())
                                WarnLog("Connection close failed: {}",
                                        closeResult.error().message());

                            return;
                        }

                        self->_writeQueue.pop_front();
                        if (const auto result = self->DoWrite(); !result.has_value())
                        {
                            WarnLog("Connection write failed: {}", result.error().message());

                            if (const auto closeResult = self->Close(); !closeResult.has_value())
                                WarnLog("Connection close failed: {}",
                                        closeResult.error().message());
                        }
                    }));
        }
        catch (const boost::system::system_error& error)
        {
            return std::unexpected(error.code());
        }

        return {};
    }

    auto Connection::Close() -> Result
    {
        _state = State::Closed;
        _writeQueue.clear();

        ErrorCode errorCode = _socket.close(errorCode);

        if (errorCode)
            return std::unexpected(errorCode);

        return {};
    }
}   // namespace Flux
