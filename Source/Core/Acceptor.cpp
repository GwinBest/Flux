#include "Acceptor.hpp"

#include <boost/asio/dispatch.hpp>
#include <boost/asio/error.hpp>
#include <boost/system/system_error.hpp>
#include <utility>

#include "Utils/Log.hpp"

namespace Flux
{
    auto Acceptor::AsyncStartAccepting() -> std::future<Result>
    {
        std::packaged_task<Result()> task([self = shared_from_this()] mutable -> Result {
            const auto acceptor = std::move(self);
            return acceptor->StartAccepting();
        });

        auto future = task.get_future();
        boost::asio::dispatch(_strand, [task = std::move(task)] mutable -> void { task(); });

        return future;
    }

    auto Acceptor::AsyncStopAccepting() -> std::future<Result>
    {
        std::packaged_task<Result()> task([self = shared_from_this()] mutable -> Result {
            const auto acceptor = std::move(self);
            return acceptor->StopAccepting();
        });

        auto future = task.get_future();
        boost::asio::dispatch(_strand, [task = std::move(task)] mutable -> void { task(); });

        return future;
    }

    auto Acceptor::CreateAcceptor(const IoContextPool::Strand& strand,
                                  const boost::asio::ip::tcp::endpoint& endpoint)
        -> std::expected<std::shared_ptr<Acceptor>, ErrorCode>
    {
        auto acceptor = std::shared_ptr<Acceptor>(new Acceptor(strand));

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

        std::size_t cancelledTimers = 0U;

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

        if (firstError)
            return std::unexpected(firstError);

        return {};
    }

    Acceptor::Acceptor(IoContextPool::Strand strand)
        : _strand(std::move(strand))
        , _acceptor(_strand)
        , _retryTimer(_strand)
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

            // TODO: handle client
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
