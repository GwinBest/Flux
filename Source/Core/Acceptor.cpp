#include "Acceptor.hpp"

#include <array>
#include <bit>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/system/system_error.hpp>
#include <limits>
#include <utility>

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

#include "Utils/Async.hpp"
#include "Utils/Log.hpp"

namespace Flux
{
    namespace
    {
        struct KeepAliveOptions final
        {
            int IdleSeconds = 60;
            int IntervalSeconds = 5;
            int ProbeCount = 3;
        };

        auto ConfigureKeepAlive(boost::asio::ip::tcp::socket& socket,
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
            keepAliveError = socket.set_option(boost::asio::socket_base::keep_alive(true),
                                               keepAliveError);

            if (keepAliveError)
                return keepAliveError;

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

            if (::setsockopt(nativeFd, SOL_TCP, TCP_KEEPINTVL, &intervalSec, sizeof(intervalSec))
                < 0)
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

            if (::setsockopt(nativeFd,
                             IPPROTO_TCP,
                             TCP_KEEPINTVL,
                             &intervalSec,
                             sizeof(intervalSec))
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
    }   // namespace

    auto Acceptor::AsyncStartAccepting() -> std::future<Result>
    {
        return DispatchOperation(_strand, [self = shared_from_this()] mutable -> Result {
            const auto acceptor = std::move(self);
            return acceptor->StartAccepting();
        });
    }

    auto Acceptor::AsyncStopAccepting() -> std::future<Result>
    {
        return DispatchOperation(_strand, [self = shared_from_this()] mutable -> Result {
            const auto acceptor = std::move(self);
            return acceptor->StopAccepting();
        });
    }

    auto Acceptor::CreateAcceptor(const IoContextPool::Strand& strand,
                                  const boost::asio::ip::tcp::endpoint& endpoint,
                                  const std::optional<std::size_t> maxConnections,
                                  const ConnectionOptions& connectionOptions,
                                  SessionFactory sessionFactory)
        -> std::expected<std::shared_ptr<Acceptor>, ErrorCode>
    {
        auto acceptor = std::shared_ptr<Acceptor>(
            new Acceptor(strand, maxConnections, connectionOptions, std::move(sessionFactory)));

        if (const auto openResult = acceptor->Open(endpoint); !openResult)
            return std::unexpected(openResult.error());

        return acceptor;
    }

    auto Acceptor::StartAccepting() -> Result
    {
        if (_status != Status::Initialized)
        {
            WarnLog("Start rejected: acceptor is not initialized");
            return std::unexpected(MakeErrorCode(Error::AcceptorNotInitialized));
        }

        _status = Status::Started;

        InfoLog("Acceptor started accepting connections");

        DoAccept();

        return {};
    }

    auto Acceptor::StopAccepting() -> Result
    {
        if (_status != Status::Initialized && _status != Status::Started)
        {
            WarnLog("Stop rejected: acceptor is not started");
            return std::unexpected(MakeErrorCode(Error::AcceptorNotStarted));
        }

        ErrorCode firstError;

        _status = Status::Stopped;

        std::size_t cancelledTimers = 0;

        try
        {
            cancelledTimers = _retryTimer.cancel();
        }
        catch (const boost::system::system_error& error)
        {
            firstError = error.code();
            WarnLog("Failed to cancel acceptor retry timer: {}", error.code().message());
        }

        if (_acceptor.is_open())
        {
            ErrorCode errorCode;
            errorCode = _acceptor.close(errorCode);
            if (errorCode)
            {
                if (!firstError)
                    firstError = errorCode;

                ErrorLog("Failed to close acceptor: {}", errorCode.message());
            }
            else
            {
                InfoLog("Acceptor stopped; cancelled_retry_timers={}", cancelledTimers);
            }
        }
        else
        {
            DebugLog("Acceptor stop requested while socket is already closed");
        }

        if (const auto result = _connectionManager.CloseAllConnections(); !result && !firstError)
        {
            firstError = result.error();
        }

        if (firstError)
            return std::unexpected(firstError);

        return {};
    }

    Acceptor::Acceptor(IoContextPool::Strand strand,
                       const std::optional<std::size_t> maxConnections,
                       const ConnectionOptions& connectionOptions,
                       SessionFactory sessionFactory)
        : _strand(std::move(strand))
        , _acceptor(_strand)
        , _retryTimer(_strand)
        , _connectionManager(connectionOptions, maxConnections, std::move(sessionFactory))
    {}

    auto Acceptor::Open(const boost::asio::ip::tcp::endpoint& endpoint) -> Result
    {
        if (_acceptor.is_open())
        {
            WarnLog("Acceptor open rejected: acceptor already open");
            return std::unexpected(MakeErrorCode(Error::AcceptorAlreadyOpen));
        }

        ErrorCode errorCode;

        errorCode = _acceptor.open(endpoint.protocol(), errorCode);
        if (errorCode)
        {
            ErrorLog("Failed to open acceptor: endpoint={}:{} error={}",
                     endpoint.address().to_string(),
                     endpoint.port(),
                     errorCode.message());
            return std::unexpected(errorCode);
        }

        errorCode = _acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true),
                                         errorCode);
        if (errorCode)
        {
            ErrorLog("Failed to configure acceptor reuse_address: endpoint={}:{} error={}",
                     endpoint.address().to_string(),
                     endpoint.port(),
                     errorCode.message());
            return std::unexpected(errorCode);
        }

        errorCode = _acceptor.bind(endpoint, errorCode);
        if (errorCode)
        {
            ErrorLog("Failed to bind acceptor: endpoint={}:{} error={}",
                     endpoint.address().to_string(),
                     endpoint.port(),
                     errorCode.message());
            return std::unexpected(errorCode);
        }

        errorCode = _acceptor.listen(boost::asio::socket_base::max_listen_connections, errorCode);
        if (errorCode)
        {
            ErrorLog("Failed to listen on acceptor: endpoint={}:{} error={}",
                     endpoint.address().to_string(),
                     endpoint.port(),
                     errorCode.message());
            return std::unexpected(errorCode);
        }

        _status = Status::Initialized;

        InfoLog("Acceptor listening: endpoint={}:{}",
                endpoint.address().to_string(),
                endpoint.port());

        return {};
    }

    auto Acceptor::DoAccept() -> void
    {
        if (_status != Status::Started || !_acceptor.is_open())
            return;

        const auto weakSelf = weak_from_this();

        _acceptor.async_accept([weakSelf](const ErrorCode& errorCode,
                                          boost::asio::ip::tcp::socket acceptedSocket) -> void {
            const auto self = weakSelf.lock();

            if (!self)
                return;

            self->OnAccept(errorCode, std::move(acceptedSocket));
        });
    }

    auto Acceptor::OnAccept(const ErrorCode& errorCode, boost::asio::ip::tcp::socket acceptedSocket)
        -> void
    {
        if (_status != Status::Started)
            return;

        if (!errorCode)
        {
            if (const auto keepAliveError = ConfigureKeepAlive(acceptedSocket))
            {
                ErrorLog("Failed to enable TCP keepalive for accepted client connection: {}",
                         keepAliveError.message());
                DoAccept();
                return;
            }

            boost::system::error_code remoteEndpointError;
            const auto remoteEndpoint = acceptedSocket.remote_endpoint(remoteEndpointError);

            if (remoteEndpointError)
            {
                InfoLog("Accepted client connection; remote endpoint unavailable: {}",
                        remoteEndpointError.message());
            }
            else
            {
                InfoLog("Accepted client connection: remote={}:{}",
                        remoteEndpoint.address().to_string(),
                        remoteEndpoint.port());
            }

            if (const auto connectionResult = _connectionManager.CreateAndStartConnection(
                    std::move(acceptedSocket));
                !connectionResult)
            {
                ErrorLog("Failed to start accepted client connection: {}",
                         connectionResult.error().message());
            }
        }
        else if (errorCode == boost::asio::error::operation_aborted)
        {
            DebugLog("Acceptor async accept cancelled");
            return;
        }
        else
        {
            WarnLog("Acceptor async accept failed: {}", errorCode.message());

            _retryTimer.expires_after(RetryInterval);

            _retryTimer.async_wait(
                [weakSelf = weak_from_this()](const ErrorCode& timerError) -> void {
                    const auto retrySelf = weakSelf.lock();
                    if (!retrySelf || retrySelf->_status != Status::Started)
                        return;

                    if (timerError == boost::asio::error::operation_aborted)
                        return;

                    if (timerError)
                    {
                        WarnLog("Acceptor retry timer failed: {}", timerError.message());
                        return;
                    }

                    retrySelf->DoAccept();
                });

            return;
        }

        DoAccept();   // Accept next client
    }
}   // namespace Flux
