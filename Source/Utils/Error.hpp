#pragma once

#include <boost/system/error_code.hpp>
#include <cstdint>
#include <expected>
#include <string>

namespace Flux
{
    enum class Error : std::uint8_t
    {
        ServerNotInitialized = 1,
        ServerNotRunning,
        ServerNotStopping,
        IoContextPoolNotStarted,
        IoContextPoolNotRunning,
        IoContextPoolStopFromWorkerThread,
        AcceptorNotInitialized,
        AcceptorNotStarted,
        AcceptorAlreadyOpen,
        IoContextPoolRunFromWorkerThread,
        ConnectionAlreadyStarted,
        ConnectionNotStarted,
        ConnectionClosed,
        ConnectionException,
        ConnectionLimitReached,
        SendQueueFull,
        NoSendCallback,
    };

    class ErrorCategory final : public boost::system::error_category
    {
    public:
        [[nodiscard]] auto name() const noexcept -> const char* override { return "flux"; }

        [[nodiscard]] auto message(const int value) const -> std::string override
        {
            switch (static_cast<Error>(value))
            {
            case Error::ServerNotInitialized:    return "server is not initialized";
            case Error::ServerNotRunning:        return "server is not running";
            case Error::ServerNotStopping:       return "server is not stopping";
            case Error::IoContextPoolNotStarted: return "IO context pool is not started";
            case Error::IoContextPoolNotRunning: return "IO context pool is not running";
            case Error::IoContextPoolStopFromWorkerThread:
                return "IO context pool cannot be stopped from one of its worker threads";
            case Error::AcceptorNotInitialized: return "acceptor is not initialized";
            case Error::AcceptorNotStarted:     return "acceptor is not started";
            case Error::AcceptorAlreadyOpen:    return "acceptor already open";
            case Error::IoContextPoolRunFromWorkerThread:
                return "IO context pool cannot be started from one of its worker threads";
            case Error::ConnectionAlreadyStarted: return "connection is already started";
            case Error::ConnectionNotStarted:     return "connection is not started";
            case Error::ConnectionClosed:         return "connection is closed";
            case Error::ConnectionException:      return "connection operation threw an exception";
            case Error::ConnectionLimitReached:   return "connection limit reached";
            case Error::SendQueueFull:            return "send queue full";
            case Error::NoSendCallback:           return "no send callback";
            default:                              return "unknown error";
            }
        }
    };

    [[nodiscard]] inline auto GetErrorCategory() noexcept -> const boost::system::error_category&
    {
        static constexpr ErrorCategory Category;
        return Category;
    }

    [[nodiscard]] inline auto MakeErrorCode(const Error error) noexcept -> boost::system::error_code
    {
        return {static_cast<int>(error), GetErrorCategory()};
    }

    using ErrorCode = boost::system::error_code;
    using Result = std::expected<void, ErrorCode>;

}   // namespace Flux
