#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/system/system_error.hpp>
#include <limits>

#ifdef _WIN32
#include <mstcpip.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <cerrno>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#endif

#include "Utils/Error.hpp"

namespace Flux
{
    struct KeepAliveOptions final
    {
        static constexpr int DefaultIdleSeconds = 60;
        static constexpr int DefaultIntervalSeconds = 5;
        static constexpr int DefaultProbeCount = 3;

        int IdleSeconds = DefaultIdleSeconds;
        int IntervalSeconds = DefaultIntervalSeconds;
        int ProbeCount = DefaultProbeCount;
    };

    inline auto ConfigureKeepAlive(boost::asio::ip::tcp::socket& socket,
                                   const KeepAliveOptions& options = {}) -> ErrorCode
    {
        if (options.IdleSeconds <= 0 || options.IntervalSeconds <= 0 || options.ProbeCount <= 0)
            return boost::asio::error::invalid_argument;

#ifdef _WIN32
        static constexpr ULONG MillisecondsPerSecond = 1000;
        static constexpr auto MaximumSeconds = std::numeric_limits<ULONG>::max()
                                             / MillisecondsPerSecond;
        if (std::cmp_greater(options.IdleSeconds, MaximumSeconds)
            || std::cmp_greater(options.IntervalSeconds, MaximumSeconds))
            return boost::asio::error::invalid_argument;
#endif

        ErrorCode keepAliveError;
        {
            ErrorCode setOptionError;
            socket.set_option(   // NOLINT(*-unused-return-value)
                boost::asio::socket_base::keep_alive(true),
                setOptionError);
            if (setOptionError)
                return setOptionError;
        }

        const auto nativeFd = socket.native_handle();

#ifdef _WIN32
        ::tcp_keepalive alive {};
        alive.onoff = 1;
        alive.keepalivetime = static_cast<ULONG>(options.IdleSeconds) * MillisecondsPerSecond;
        alive.keepaliveinterval = static_cast<ULONG>(options.IntervalSeconds)
                                * MillisecondsPerSecond;

        DWORD bytesReturned = 0;
        if (::WSAIoctl(nativeFd,
                       SIO_KEEPALIVE_VALS,
                       &alive,
                       sizeof(alive),
                       nullptr,
                       0,
                       &bytesReturned,
                       nullptr,
                       nullptr)
            == SOCKET_ERROR)
        {
            keepAliveError = boost::system::error_code(WSAGetLastError(),
                                                       boost::system::system_category());
            return keepAliveError;
        }

#ifdef TCP_KEEPCNT
        // SIO_KEEPALIVE_VALS does not configure the probe count (Windows 10 1703+).
        const auto count = std::bit_cast<std::array<char, sizeof(DWORD)>>(
            static_cast<DWORD>(options.ProbeCount));
        if (::setsockopt(nativeFd,
                         IPPROTO_TCP,
                         TCP_KEEPCNT,
                         count.data(),
                         static_cast<int>(count.size()))
            == SOCKET_ERROR)
        {
            return {::WSAGetLastError(), boost::system::system_category()};
        }
#endif
#elifdef __linux__
        const int idleSec = options.IdleSeconds;
        const int intervalSec = options.IntervalSeconds;
        const int count = options.ProbeCount;
        if (::setsockopt(nativeFd, SOL_TCP, TCP_KEEPIDLE, &idleSec, sizeof(idleSec)) < 0)
        {
            keepAliveError = boost::system::error_code(errno, boost::system::system_category());
            return keepAliveError;
        }

        if (::setsockopt(nativeFd, SOL_TCP, TCP_KEEPINTVL, &intervalSec, sizeof(intervalSec)) < 0)
        {
            keepAliveError = boost::system::error_code(errno, boost::system::system_category());
            return keepAliveError;
        }

        if (::setsockopt(nativeFd, SOL_TCP, TCP_KEEPCNT, &count, sizeof(count)) < 0)
        {
            keepAliveError = boost::system::error_code(errno, boost::system::system_category());
            return keepAliveError;
        }

#elifdef __APPLE__
        const int idleSec = options.IdleSeconds;
        const int intervalSec = options.IntervalSeconds;
        const int count = options.ProbeCount;
        if (::setsockopt(nativeFd, IPPROTO_TCP, TCP_KEEPALIVE, &idleSec, sizeof(idleSec)) < 0)
        {
            keepAliveError = boost::system::error_code(errno, boost::system::system_category());
            return keepAliveError;
        }

        if (::setsockopt(nativeFd, IPPROTO_TCP, TCP_KEEPINTVL, &intervalSec, sizeof(intervalSec))
            < 0)
        {
            keepAliveError = boost::system::error_code(errno, boost::system::system_category());
            return keepAliveError;
        }

        if (::setsockopt(nativeFd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count)) < 0)
        {
            keepAliveError = boost::system::error_code(errno, boost::system::system_category());
            return keepAliveError;
        }
#else
        return boost::asio::error::operation_not_supported;
#endif

        return keepAliveError;
    }
}   // namespace Flux
